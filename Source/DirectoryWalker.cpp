
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker.h"
#include "DirectoryWalker_Util.h"
#include "HashFactory.h"

#define DDUP_NO_MATCH       0
#define DDUP_SOME_MATCH     1
#define DDUP_ALL_MATCH      2

#define INIT_DUP_WITHIN_SIZE    32

static BOOL AddToDupWithinList(_In_ PDUPFILES_WITHIN pDupWithin, _In_ PFILEINFO pFileInfo);
static PFILEINFO FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ PDUPFILES_WITHIN pDupWithinToSearch, _Inout_ int* piStartIndex);
static BOOL RemoveFromDupWithinList(_In_ PFILEINFO pFileToDelete, _In_ PDUPFILES_WITHIN pDupWithinToSearch);

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo);
static BOOL _DeleteFileUpdateDir(_In_ PFILEINFO pFileToDelete, _In_ PDIRINFO pDeleteFrom, _In_opt_ PDIRINFO pUpdateDir);

static HRESULT _Init(_In_ PCWSTR pszFolderpath, _In_ BOOL fRecursive, _Out_ PDIRINFO* ppDirInfo)
{
    HRESULT hr = S_OK;
    PDIRINFO pDirInfo = (PDIRINFO)malloc(sizeof(DIRINFO));
    if (pDirInfo == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto error_return;
    }

    ZeroMemory(pDirInfo, sizeof(*pDirInfo));
    wcscpy_s(pDirInfo->pszPath, ARRAYSIZE(pDirInfo->pszPath), pszFolderpath);
    int nEstEntries = fRecursive ? 2048 : 256;
    if (FAILED(CHL_DsCreateHT(&pDirInfo->phtFiles, nEstEntries, CHL_KT_WSTRING, CHL_VT_POINTER, TRUE)))
    {
        logerr(L"Couldn't create hash table for dir: %s", pszFolderpath);
        hr = E_FAIL;
        goto error_return;
    }

    // Allocate an array of pointer for the dup within files
    hr = CHL_DsCreateRA(&pDirInfo->stDupFilesInTree.aFiles, CHL_VT_USEROBJECT, INIT_DUP_WITHIN_SIZE, (MAXINT / 2));
    if (FAILED(hr))
    {
        logerr(L"Couldn't create resizable array for dup files, hr: %x", hr);
        goto error_return;
    }

    *ppDirInfo = pDirInfo;
    return hr;

error_return:
    *ppDirInfo = NULL;
    if (pDirInfo != NULL)
    {
        DestroyDirInfo_NoHash(pDirInfo);
    }
    return hr;
}

void DestroyDirInfo_NoHash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    if (pDirInfo->phtFiles != NULL)
    {
        CHL_DsDestroyHT(pDirInfo->phtFiles);
    }

    CHL_DsDestroyRA(&pDirInfo->stDupFilesInTree.aFiles);

    free(pDirInfo);
}

// Build a tree 
BOOL BuildDirTree_NoHash(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir)
{
    PCHL_QUEUE pqDirsToTraverse;
    if (FAILED(CHL_DsCreateQ(&pqDirsToTraverse, CHL_VT_POINTER, 20)))
    {
        logerr(L"Could not create queue for BFS. Rootpath: %s", pszRootpath);
        goto error_return;
    }

    loginfo(L"Starting traversal for root dir: %s", pszRootpath);

    // Init the first dir to traverse. 
    // ** IMP: pFirst must be NULL here so that a new object is created in callee
    PDIRINFO pFirstDir = NULL;
    if (!BuildFilesInDir_NoHash(pszRootpath, pqDirsToTraverse, &pFirstDir))
    {
        logerr(L"Could not build files in dir: %s", pszRootpath);
        goto error_return;
    }

    PDIRINFO pDirToTraverse;
    while (SUCCEEDED(pqDirsToTraverse->Delete(pqDirsToTraverse, &pDirToTraverse, NULL, FALSE)))
    {
        loginfo(L"Continuing traversal in dir: %s", pDirToTraverse->pszPath);
        if (!BuildFilesInDir_NoHash(pDirToTraverse->pszPath, pqDirsToTraverse, &pFirstDir))
        {
            logerr(L"Could not build files in dir: %s. Continuing...", pDirToTraverse->pszPath);
        }
    }

    pqDirsToTraverse->Destroy(pqDirsToTraverse);

    *ppRootDir = pFirstDir;
    return TRUE;

error_return:
    if (pqDirsToTraverse)
    {
        pqDirsToTraverse->Destroy(pqDirsToTraverse);
    }
    *ppRootDir = NULL;
    return FALSE;
}

// Build the list of files in the given folder.
BOOL BuildFilesInDir_NoHash(
    _In_ PCWSTR pszFolderpath,
    _In_opt_ PCHL_QUEUE pqDirsToTraverse,
    _Inout_ PDIRINFO* ppDirInfo)
{
    SB_ASSERT(pszFolderpath);
    SB_ASSERT(ppDirInfo);

    HANDLE hFindFile = NULL;

    loginfo(L"Building dir: %s", pszFolderpath);
    if (*ppDirInfo == NULL && FAILED(_Init(pszFolderpath, (pqDirsToTraverse != NULL), ppDirInfo)))
    {
        logerr(L"Init failed for dir: %s", pszFolderpath);
        goto error_return;
    }

    // Derefernce just to make it easier to code
    PDIRINFO pCurDirInfo = *ppDirInfo;

    // In order to list all files within the specified directory,
    // path sent to FindFirstFile must end with a "\\*"
    WCHAR szSearchpath[MAX_PATH] = L"";
    wcscpy_s(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath);

    int nLen = wcsnlen(pszFolderpath, MAX_PATH);
    if (nLen > 2 && wcsncmp(pszFolderpath + nLen - 2, L"\\*", MAX_PATH) != 0)
    {
        PathCchCombine(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath, L"*");
    }

    // Initialize search for files in folder
    WIN32_FIND_DATA findData;
    hFindFile = FindFirstFile(szSearchpath, &findData);
    if (hFindFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        // No files found under the folder. Just return.
        return TRUE;
    }

    if (hFindFile == INVALID_HANDLE_VALUE)
    {
        logerr(L"FindFirstFile().");
        goto error_return;
    }

    do
    {
        // Skip banned files and folders
        if (IsFileFolderBanned(findData.cFileName, ARRAYSIZE(findData.cFileName)))
        {
            continue;
        }

        szSearchpath[0] = 0;
        if (FAILED(PathCchCombine(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath, findData.cFileName) == NULL))
        {
            logerr(L"PathCchCombine() failed for %s and %s", pszFolderpath, findData.cFileName);
            goto error_return;
        }

        PFILEINFO pFileInfo;
        if (!CreateFileInfo(szSearchpath, FALSE, &pFileInfo))
        {
            // Treat as warning and move on.
            logwarn(L"Unable to get file info for: %s", szSearchpath);
            continue;
        }

        if (pFileInfo->fIsDirectory && pqDirsToTraverse)
        {
            // If pqDirsToTraverse is not null, it means caller wants recursive directory traversal
            PDIRINFO pSubDir;
            if (FAILED(_Init(szSearchpath, TRUE, &pSubDir)))
            {
                logwarn(L"Unable to init dir info for: %s", szSearchpath);
                free(pFileInfo);
                continue;
            }

            // Insert pSubDir into the queue so that it will be traversed later
            if (FAILED(pqDirsToTraverse->Insert(pqDirsToTraverse, pSubDir, sizeof pSubDir)))
            {
                logwarn(L"Unable to add sub dir [%s] to traversal queue, cur dir: %s", findData.cFileName, pszFolderpath);
                free(pFileInfo);
                continue;
            }
            ++(pCurDirInfo->nDirs);
        }
        else
        {
            // Either this is a file or a directory but the folder must be considered as a file
            // because recursion is not enabled and we want to enable comparison of some attributes of a folder.
            // If the current file's name is already inserted, then add it to the dup within list
            BOOL fFileAdded;
            if (SUCCEEDED(CHL_DsFindHT(pCurDirInfo->phtFiles, findData.cFileName,
                StringSizeBytes(findData.cFileName), NULL, NULL, TRUE)))
            {
                fFileAdded = AddToDupWithinList(&pCurDirInfo->stDupFilesInTree, pFileInfo);
            }
            else
            {
                fFileAdded = SUCCEEDED(CHL_DsInsertHT(pCurDirInfo->phtFiles, findData.cFileName,
                    StringSizeBytes(findData.cFileName), pFileInfo, sizeof pFileInfo));
            }

            if (!fFileAdded)
            {
                logerr(L"Cannot add %s to file list: %s", (pFileInfo->fIsDirectory ? L"dir" : L"file"), findData.cFileName);
                free(pFileInfo);
            }
            else
            {
                pFileInfo->fIsDirectory ? ++(pCurDirInfo->nDirs) : ++(pCurDirInfo->nFiles);
                logdbg(L"Added %s: %s", (pFileInfo->fIsDirectory ? L"dir" : L"file"), findData.cFileName);
            }
        }
    } while (FindNextFile(hFindFile, &findData));

    if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        logerr(L"Failed in enumerating files in directory: %s", pszFolderpath);
        goto error_return;
    }

    FindClose(hFindFile);
    return TRUE;

error_return:
    if (hFindFile != INVALID_HANDLE_VALUE)
    {
        FindClose(hFindFile);
    }

    if (*ppDirInfo != NULL)
    {
        DestroyDirInfo_NoHash(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return FALSE;
}

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles_NoHash(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir)
{
    SB_ASSERT(pLeftDir);
    SB_ASSERT(pRightDir);

    // For each file in the left dir, search for the same in the right dir
    // If found, pass both to the CompareFileInfoAndMark().

    CHL_HT_ITERATOR itrLeft;
    if (FAILED(CHL_DsInitIteratorHT(pLeftDir->phtFiles, &itrLeft)))
    {
        return TRUE;
    }

    WCHAR* pszLeftFile;
    PFILEINFO pLeftFile, pRightFile;
    int nLeftKeySize;

    logdbg(L"Comparing dirs: %s and %s", pLeftDir->pszPath, pRightDir->pszPath);
    do
    {
        itrLeft.GetCurrent(&itrLeft, &pszLeftFile, &nLeftKeySize, &pLeftFile, NULL, TRUE);
        if (SUCCEEDED(CHL_DsFindHT(pRightDir->phtFiles, (void*)pszLeftFile, nLeftKeySize, &pRightFile, NULL, TRUE)))
        {
            // Same file found in right dir, compare and mark as duplicate
            if (CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"Duplicate %s: %s", (pLeftFile->fIsDirectory ? L"dir" : L"file"), pszLeftFile);
            }
        }

        int index = 0;
        while ((pRightFile = FindInDupWithinList(pLeftFile->szFilename, &pRightDir->stDupFilesInTree, &index)) != NULL)
        {
            // Same file found in right dir, compare and mark as duplicate
            if (CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"DupWithin Duplicate %s: %s", (pLeftFile->fIsDirectory ? L"dir" : L"file"), pszLeftFile);
            }
        }
    } while (SUCCEEDED(itrLeft.MoveNext(&itrLeft)));

    // See if dup within files are duplicate
    for (int i = 0; i < pLeftDir->stDupFilesInTree.nCurFiles; ++i)
    {
        // Check if this file is in the right dir by checking both its hashtable and the dupwithin list
        if (FAILED(CHL_DsReadRA(&pLeftDir->stDupFilesInTree.aFiles, i, &pLeftFile, NULL, TRUE)))
        {
            continue;
        }

        // Hashtable first
        if (SUCCEEDED(CHL_DsFindHT(pRightDir->phtFiles, pLeftFile->szFilename, StringSizeBytes(pLeftFile->szFilename), &pRightFile, NULL, TRUE)))
        {
            // Same file found in right dir, compare and mark as duplicate
            if (CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"Duplicate file: %s", pLeftFile->szFilename);
            }
        }

        // Now, the dup-within
        int index = 0;
        while ((pRightFile = FindInDupWithinList(pLeftFile->szFilename, &pRightDir->stDupFilesInTree, &index)) != NULL)
        {
            // Same file found in right dir, compare and mark as duplicate
            if (CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"DupWithin Duplicate file: %s", pLeftFile->szFilename);
            }
        }
    }

    return TRUE;
}

#pragma region FileOperations

void ClearFilesDupFlag_NoHash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);
    if (pDirInfo->nFiles > 0)
    {
        CHL_HT_ITERATOR itr;
        if (SUCCEEDED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
        {
            PFILEINFO pFileInfo;
            int nValSize;
            while (SUCCEEDED(itr.GetCurrent(&itr, NULL, NULL, &pFileInfo, &nValSize, TRUE)))
            {
                ClearDuplicateAttr(pFileInfo);
                (void)itr.MoveNext(&itr);
            }
        }
    }
}

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo)
{
    BOOL fRetVal = TRUE;

    WCHAR szFilepath[MAX_PATH];
    loginfo(L"Deleting file = %s\\%s", pFileInfo->szPath, pFileInfo->szFilename);
    if (FAILED(PathCchCombine(szFilepath, ARRAYSIZE(szFilepath), pFileInfo->szPath, pFileInfo->szFilename) == NULL))
    {
        logerr(L"PathCchCombine() failed for %s + %s", pFileInfo->szPath, pFileInfo->szFilename);
        fRetVal = FALSE;
        goto done;
    }

    if (!DeleteFile(szFilepath))
    {
        logerr(L"DeleteFile failed, err: %u", GetLastError());
        fRetVal = FALSE;
    }

    // Remove from file list
    if (FAILED(CHL_DsRemoveHT(pDirInfo->phtFiles, pFileInfo->szFilename, StringSizeBytes(pFileInfo->szFilename))))
    {
        // See if file is present in the dup within list
        if (!(RemoveFromDupWithinList(pFileInfo, &pDirInfo->stDupFilesInTree)))
        {
            logerr(L"Failed to remove file from hashtable/list: %s", pFileInfo->szFilename);
            fRetVal = FALSE;
            goto done;
        }
    }

    --(pDirInfo->nFiles);

done:
    return fRetVal;
}

BOOL _DeleteFileUpdateDir(_In_ PFILEINFO pFileToDelete, _In_ PDIRINFO pDeleteFrom, _In_opt_ PDIRINFO pUpdateDir)
{
    SB_ASSERT(pFileToDelete->fIsDirectory == FALSE);
    if (pUpdateDir)
    {
        // Find this file in the other directory and update that file info
        // to say that it is not a duplicate any more.
        PFILEINFO pFileToUpdate;
        BOOL fFound = SUCCEEDED(CHL_DsFindHT(pUpdateDir->phtFiles, pFileToDelete->szFilename,
            StringSizeBytes(pFileToDelete->szFilename), &pFileToUpdate, NULL, TRUE));

        // Must find in the other dir also.
        SB_ASSERT(fFound);
        ClearDuplicateAttr(pFileToUpdate);
    }

    // Now, delete file from the specified dir and from file lists
    return _DeleteFile(pDeleteFrom, pFileToDelete);
}

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir_NoHash(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate)
{
    SB_ASSERT(pDirDeleteFrom);

    loginfo(L"In folder = %s", pDirDeleteFrom->pszPath);
    if (pDirDeleteFrom->nFiles <= 0)
    {
        logwarn(L"Empty folder");
        goto error_return;
    }

    CHL_HT_ITERATOR itr;
    if (FAILED(CHL_DsInitIteratorHT(pDirDeleteFrom->phtFiles, &itr)))
    {
        goto error_return;
    }

    PCHL_HTABLE phtFoldersSeen;
    if (FAILED(DelEmptyFolders_Init(pDirDeleteFrom, &phtFoldersSeen)))
    {
        goto error_return;
    }

    PFILEINFO pFileInfo = NULL;
    PFILEINFO pPrevDupFileInfo = NULL;
    while (SUCCEEDED(itr.GetCurrent(&itr, NULL, NULL, &pFileInfo, NULL, TRUE)))
    {
        (void)itr.MoveNext(&itr);

        DelEmptyFolders_Add(phtFoldersSeen, pFileInfo);

        if (pFileInfo->fIsDirectory == TRUE)
        {
            continue;
        }

        // The hashtable iterator expects the currently found entry
        // not to be removed until we move to the next entry. Hence,
        // this logic with previous pointer.
        if (pPrevDupFileInfo)
        {
            _DeleteFileUpdateDir(pPrevDupFileInfo, pDirDeleteFrom, pDirToUpdate);
            pPrevDupFileInfo = NULL;
        }

        if (IsDuplicateFile(pFileInfo))
        {
            pPrevDupFileInfo = pFileInfo;
        }
    }

    if (pPrevDupFileInfo)
    {
        _DeleteFileUpdateDir(pPrevDupFileInfo, pDirDeleteFrom, pDirToUpdate);
        pPrevDupFileInfo = NULL;
    }

    DelEmptyFolders_Delete(phtFoldersSeen);
    return TRUE;

error_return:
    DelEmptyFolders_Delete(phtFoldersSeen);
    return FALSE;
}

// Similar to the DeleteDupFilesInDir() function but it deletes only the specified files
// from the pDirDeleteFrom directory and update the other directory files'. The files to
// be deleted are specified in a list of strings that are the file names.
// ** Assuming that paszFileNamesToDelete is an array of MAX_PATH strings.
BOOL DeleteFilesInDir_NoHash(
    _In_ PDIRINFO pDirDeleteFrom,
    _In_ PCWSTR paszFileNamesToDelete,
    _In_ int nFileNames,
    _In_opt_ PDIRINFO pDirToUpdate)
{
    SB_ASSERT(pDirDeleteFrom);
    SB_ASSERT(paszFileNamesToDelete);
    SB_ASSERT(nFileNames >= 0);

    int index = 0;
    loginfo(L"Deleting %d files from %s", nFileNames, pDirDeleteFrom->pszPath);

    PCHL_HTABLE phtFoldersSeen;
    if (FAILED(DelEmptyFolders_Init(pDirDeleteFrom, &phtFoldersSeen)))
    {
        return FALSE;
    }

    while (index < nFileNames)
    {
        PCWSTR pszFileName = paszFileNamesToDelete + (MAX_PATH * index);
        ++index;

        PFILEINFO pFileToDelete;
        if (SUCCEEDED(CHL_DsFindHT(pDirDeleteFrom->phtFiles, (PCVOID)pszFileName,
            StringSizeBytes(pszFileName), &pFileToDelete, NULL, TRUE)))
        {
            DelEmptyFolders_Add(phtFoldersSeen, pFileToDelete);

            if (pFileToDelete->fIsDirectory == FALSE)
            {
                _DeleteFileUpdateDir(pFileToDelete, pDirDeleteFrom, pDirToUpdate);
            }
        }
        else
        {
            logwarn(L"Couldn't find file %s in dir %s", pszFileName, pDirDeleteFrom->pszPath);
        }
    }

    DelEmptyFolders_Delete(phtFoldersSeen);
    return TRUE;
}

#pragma endregion FileOperations

// Print the dir tree in BFS order, two blank lines separating
// file listing of each directory
void PrintDirTree_NoHash(_In_ PDIRINFO pRootDir)
{
    PrintFilesInDir(pRootDir);
    wprintf(L"\n");
    // Print any dup within files
    if (pRootDir->stDupFilesInTree.nCurFiles > 0)
    {
        wprintf(L"These are the files that are duplicate within the specified root folder's tree itself:\n");
        for (int i = 0; i < pRootDir->stDupFilesInTree.nCurFiles; ++i)
        {
            PFILEINFO pFileInfo;
            if (FAILED(CHL_DsReadRA(&pRootDir->stDupFilesInTree.aFiles, i, &pFileInfo, NULL, TRUE)))
            {
                continue;
            }

            wprintf(L"%10u %c %1d %s\n",
                pFileInfo->llFilesize.LowPart,
                pFileInfo->fIsDirectory ? L'D' : L'F',
                IsDuplicateFile(pFileInfo) ? 1 : 0,
                pFileInfo->szFilename);
        }
    }
    return;
}

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir_NoHash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    CHL_HT_ITERATOR itr;
    if (FAILED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
    {
        return;
    }

    wprintf(L"%s\n", pDirInfo->pszPath);

    PFILEINFO pFileInfo = NULL;
    while (SUCCEEDED(itr.GetCurrent(&itr, NULL, NULL, &pFileInfo, NULL, TRUE)))
    {
        (void)itr.MoveNext(&itr);
        wprintf(L"%10u %c %1d %s\n",
            pFileInfo->llFilesize.LowPart,
            pFileInfo->fIsDirectory ? L'D' : L'F',
            IsDuplicateFile(pFileInfo) ? 1 : 0,
            pFileInfo->szFilename);
    }

    return;
}

BOOL AddToDupWithinList(_In_ PDUPFILES_WITHIN pDupWithin, _In_ PFILEINFO pFileInfo)
{
    HRESULT hr = pDupWithin->aFiles.Write(&pDupWithin->aFiles, pDupWithin->nCurFiles, pFileInfo, sizeof(*pFileInfo));
    if (SUCCEEDED(hr))
    {
        ++(pDupWithin->nCurFiles);
    }
    return SUCCEEDED(hr);
}

PFILEINFO FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ PDUPFILES_WITHIN pDupWithinToSearch, _Inout_ int* piStartIndex)
{
    SB_ASSERT(0 <= *piStartIndex && *piStartIndex <= pDupWithinToSearch->nCurFiles);

    while (*piStartIndex < pDupWithinToSearch->nCurFiles)
    {
        ++(*piStartIndex);

        PFILEINFO pFile;
        if (FAILED(CHL_DsReadRA(&pDupWithinToSearch->aFiles, (*piStartIndex) - 1, &pFile, NULL, TRUE)))
        {
            continue;
        }

        if (_wcsnicmp(pszFilename, pFile->szFilename, MAX_PATH) == 0)
        {
            return pFile;
        }
    }
    return NULL;
}

BOOL RemoveFromDupWithinList(_In_ PFILEINFO pFileToDelete, _In_ PDUPFILES_WITHIN pDupWithinToSearch)
{
    WCHAR szCompareFromPath[MAX_PATH];
    if (FAILED(PathCchCombine(szCompareFromPath, ARRAYSIZE(szCompareFromPath), pFileToDelete->szPath, pFileToDelete->szFilename) == NULL))
    {
        logerr(L"PathCchCombine() failed for %s + %s", pFileToDelete->szPath, pFileToDelete->szFilename);
        return FALSE;
    }

    WCHAR szCompareToPath[MAX_PATH];
    for (int i = 0; i < pDupWithinToSearch->nCurFiles; ++i)
    {
        PFILEINFO pFile;
        if (FAILED(CHL_DsReadRA(&pDupWithinToSearch->aFiles, i, &pFile, NULL, TRUE)))
        {
            continue;
        }

        if (FAILED(PathCchCombine(szCompareToPath, ARRAYSIZE(szCompareToPath), pFile->szPath, pFile->szFilename) == NULL))
        {
            logerr(L"PathCchCombine() failed for %s + %s", pFile->szPath, pFile->szFilename);
            return FALSE;
        }

        if (_wcsnicmp(szCompareFromPath, szCompareToPath, MAX_PATH) == 0)
        {
            CHL_DsClearAtRA(&pDupWithinToSearch->aFiles, i);
            return TRUE;
        }
    }
    return FALSE;
}
