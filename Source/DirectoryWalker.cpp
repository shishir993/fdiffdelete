
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
static int FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ PDUPFILES_WITHIN pDupWithinToSearch, _In_ int iStartIndex);
static BOOL RemoveFromDupWithinList(_In_ PFILEINFO pFileToDelete, _In_ PDUPFILES_WITHIN pDupWithinToSearch);
static BOOL CompareDirsAndMarkFiles(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir);

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo);
static BOOL _DeleteFileUpdateDir(_In_ PFILEINFO pFileToDelete, _In_ PDIRINFO pDeleteFrom, _In_opt_ PDIRINFO pUpdateDir);

static HRESULT _Init(_In_ PCWSTR pszFolderpath, _In_ BOOL fRecursive, _Out_ PDIRINFO* ppDirInfo)
{
    HRESULT hr = S_OK;
    PDIRINFO pDirInfo = (PDIRINFO)malloc(sizeof(DIRINFO));
    if(pDirInfo == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto error_return;
    }

    ZeroMemory(pDirInfo, sizeof(*pDirInfo));
    wcscpy_s(pDirInfo->pszPath, ARRAYSIZE(pDirInfo->pszPath), pszFolderpath);
    int nEstEntries = fRecursive ? 2048 : 256;
    if(FAILED(CHL_DsCreateHT(&pDirInfo->phtFiles, nEstEntries, CHL_KT_STRING, CHL_VT_POINTER, TRUE)))
    {
        logerr(L"Couldn't create hash table for dir: %s", pszFolderpath);
        hr = E_FAIL;
        goto error_return;
    }

    // Allocate an array of pointer for the dup within files
    pDirInfo->stDupFilesInTree.nCurSize = INIT_DUP_WITHIN_SIZE;
    pDirInfo->stDupFilesInTree.apFiles = (PFILEINFO*)malloc(sizeof(PFILEINFO) * INIT_DUP_WITHIN_SIZE);
    if(pDirInfo->stDupFilesInTree.apFiles == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto error_return;
    }

    *ppDirInfo = pDirInfo;
    return hr;

error_return:
    *ppDirInfo = NULL;
    if(pDirInfo != NULL)
    {
        DestroyDirInfo_NoHash(pDirInfo);
    }
    return hr;
}

void DestroyDirInfo_NoHash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    if(pDirInfo->phtFiles != NULL)
    {
        CHL_DsDestroyHT(pDirInfo->phtFiles);
    }

    // Free each FILEINFO stored in the dup within list
    for(int i = 0; i < pDirInfo->stDupFilesInTree.nCurFiles; ++i)
    {
        PFILEINFO pFile = pDirInfo->stDupFilesInTree.apFiles[i];
        if(pFile != NULL)
        {
            free(pFile);
        }
    }

    // Finally, free the list of FILEINFO pointers itself
    if(pDirInfo->stDupFilesInTree.apFiles != NULL)
    {
        free(pDirInfo->stDupFilesInTree.apFiles);
    }

    free(pDirInfo);
}

// Build a tree 
BOOL BuildDirTree_NoHash(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir)
{
    PCHL_QUEUE pqDirsToTraverse;
    if(FAILED(CHL_DsCreateQ(&pqDirsToTraverse, CHL_VT_POINTER, 20)))
    {
        logerr(L"Could not create queue for BFS. Rootpath: %s", pszRootpath);
        goto error_return;
    }

    loginfo(L"Starting traversal for root dir: %s", pszRootpath);

    // Init the first dir to traverse. 
    // ** IMP: pFirst must be NULL here so that a new object is created in callee
    PDIRINFO pFirstDir = NULL;
    if(!BuildFilesInDir_NoHash(pszRootpath, pqDirsToTraverse, &pFirstDir))
    {
        logerr(L"Could not build files in dir: %s", pszRootpath);
        goto error_return;
    }

    PDIRINFO pDirToTraverse;
    while(SUCCEEDED(pqDirsToTraverse->Delete(pqDirsToTraverse, &pDirToTraverse, NULL, FALSE)))
    {
        loginfo(L"Continuing traversal in dir: %s", pDirToTraverse->pszPath);
        if(!BuildFilesInDir_NoHash(pDirToTraverse->pszPath, pqDirsToTraverse, &pFirstDir))
        {
            logerr(L"Could not build files in dir: %s. Continuing...", pDirToTraverse->pszPath);
        }
    }

    pqDirsToTraverse->Destroy(pqDirsToTraverse);

    *ppRootDir = pFirstDir;
    return TRUE;

error_return:
    if(pqDirsToTraverse)
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

    loginfo(L"Building dir: %s", pszFolderpath);
    if(*ppDirInfo == NULL && FAILED(_Init(pszFolderpath, (pqDirsToTraverse != NULL), ppDirInfo)))
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
    if(nLen > 2 && wcsncmp(pszFolderpath + nLen - 2, L"\\*", MAX_PATH) != 0)
    {
        wcscpy_s(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath);
        wcscat_s(szSearchpath, ARRAYSIZE(szSearchpath), L"\\*");
    }

    // Initialize search for files in folder
    WIN32_FIND_DATA findData;
    HANDLE hFindFile = FindFirstFile(szSearchpath, &findData);
    if(hFindFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        // No files found under the folder. Just return.
        return TRUE;
    }

    if(hFindFile == INVALID_HANDLE_VALUE)
    {
        logerr(L"FindFirstFile().");
        goto error_return;
    }

    do
    {
        // Skip banned files and folders
        if(IsFileFolderBanned(findData.cFileName, ARRAYSIZE(findData.cFileName)))
        {
            continue;
        }

        szSearchpath[0] = 0;
        if(PathCombine(szSearchpath, pszFolderpath, findData.cFileName) == NULL)
        {
            logerr(L"PathCombine() failed for %s and %s", pszFolderpath, findData.cFileName);
            goto error_return;
        }

        PFILEINFO pFileInfo;
        if(!CreateFileInfo(szSearchpath, FALSE, &pFileInfo))
        {
            // Treat as warning and move on.
            logwarn(L"Unable to get file info for: %s", szSearchpath);
            continue;
        }

        if(pFileInfo->fIsDirectory)
        {
            // If pqDirsToTraverse is not null, it means caller wants recursive directory traversal
            if(pqDirsToTraverse)
            {
                PDIRINFO pSubDir;
                if(FAILED(_Init(szSearchpath, TRUE, &pSubDir)))
                {
                    logwarn(L"Unable to init dir info for: %s", szSearchpath);
                    free(pFileInfo);
                    continue;
                }

                // Insert pSubDir into the queue so that it will be traversed later
                if(FAILED(pqDirsToTraverse->Insert(pqDirsToTraverse, pSubDir, sizeof pSubDir)))
                {
                    logwarn(L"Unable to add sub dir [%s] to traversal queue, cur dir: %s", findData.cFileName, pszFolderpath);
                    free(pFileInfo);
                    continue;
                }
                ++(pCurDirInfo->nDirs);
            }
            else
            {
                // Ignore directories when recursive mode is turned OFF
                free(pFileInfo);
                logdbg(L"Skipped adding dir: %s", findData.cFileName);
            }
        }
        else
        {
            // The hashtable supports only char* keys
            char szKey[MAX_PATH];
            _ConvertToAscii(findData.cFileName, szKey);
            int nSize = strnlen_s(szKey, MAX_PATH) + 1;

            // If the current file's name is already inserted, then add it to the
            // dup within list
            BOOL fFileAdded = TRUE;
            if(SUCCEEDED(CHL_DsFindHT(pCurDirInfo->phtFiles, szKey, nSize, NULL, NULL, TRUE)))
            {
                fFileAdded = AddToDupWithinList(&pCurDirInfo->stDupFilesInTree, pFileInfo);
            }
            else
            {
                fFileAdded = SUCCEEDED(CHL_DsInsertHT(pCurDirInfo->phtFiles, szKey, nSize, pFileInfo, sizeof pFileInfo));
            }

            if(!fFileAdded)
            {
                logerr(L"Cannot add file to file list: %s", findData.cFileName);
                free(pFileInfo);
            }
            else
            {
                ++(pCurDirInfo->nFiles);
                logdbg(L"Added file: %s", findData.cFileName);
            }
        }
    } while(FindNextFile(hFindFile, &findData));

    if(GetLastError() != ERROR_NO_MORE_FILES)
    {
        logerr(L"Failed in enumerating files in directory: %s", pszFolderpath);
        goto error_return;
    }

    FindClose(hFindFile);
    return TRUE;

error_return:
    if(hFindFile != INVALID_HANDLE_VALUE)
    {
        FindClose(hFindFile);
    }

    if(*ppDirInfo != NULL)
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

    // Trivial check for directory to be empty
    if(pLeftDir->nFiles <= 0 || pRightDir->nFiles <= 0)
    {
        return TRUE;
    }

    // For each file in the left dir, search for the same in the right dir
    // If found, pass both to the CompareFileInfoAndMark().

    CHL_HT_ITERATOR itrLeft;
    if(FAILED(CHL_DsInitIteratorHT(pLeftDir->phtFiles, &itrLeft)))
    {
        logerr(L"Cannot iterate through file list for dir: %s", pLeftDir->pszPath);
        goto error_return;
    }

    char* pszLeftFile;
    PFILEINFO pLeftFile, pRightFile;
    int nLeftKeySize;

    logdbg(L"Comparing dirs: %s and %s", pLeftDir->pszPath, pRightDir->pszPath);
    while(SUCCEEDED(CHL_DsGetNextHT(&itrLeft, &pszLeftFile, &nLeftKeySize, &pLeftFile, NULL, TRUE)))
    {
        if(SUCCEEDED(CHL_DsFindHT(pRightDir->phtFiles, (void*)pszLeftFile, nLeftKeySize, &pRightFile, NULL, TRUE)))
        {
            // Same file found in right dir, compare and mark as duplicate
            if(CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"Duplicate file: %S", pszLeftFile);
            }
        }

        int index = -1;
        while((index = FindInDupWithinList(pLeftFile->szFilename, &pRightDir->stDupFilesInTree, index + 1)) != -1)
        {
            // Same file found in right dir, compare and mark as duplicate
            pRightFile = pRightDir->stDupFilesInTree.apFiles[index];
            if(CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"DupWithin Duplicate file: %S", pszLeftFile);
            }
        }
    }

    // See if dup within files are duplicate
    for(int i = 0; i < pLeftDir->stDupFilesInTree.nCurFiles; ++i)
    {
        // Check if this file is in the right dir by checking both its hashtable and the dupwithin list
        pLeftFile = pLeftDir->stDupFilesInTree.apFiles[i];

        // Hashtable first
        char szKey[MAX_PATH];
        _ConvertToAscii(pLeftFile->szFilename, szKey);
        nLeftKeySize = strnlen_s(szKey, MAX_PATH) + 1;
        if(SUCCEEDED(CHL_DsFindHT(pRightDir->phtFiles, (void*)szKey, nLeftKeySize, &pRightFile, NULL, TRUE)))
        {
            // Same file found in right dir, compare and mark as duplicate
            if(CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"Duplicate file: %s", pLeftFile->szFilename);
            }
        }

        // Now, the dup-within
        int index = -1;
        while((index = FindInDupWithinList(pLeftFile->szFilename, &pRightDir->stDupFilesInTree, index + 1)) != -1)
        {
            // Same file found in right dir, compare and mark as duplicate
            pRightFile = pRightDir->stDupFilesInTree.apFiles[index];
            if(CompareFileInfoAndMark(pLeftFile, pRightFile, FALSE))
            {
                logdbg(L"DupWithin Duplicate file: %s", pLeftFile->szFilename);
            }
        }
    }

    return TRUE;

error_return:
    return FALSE;
}

#pragma region FileOperations

void ClearFilesDupFlag_NoHash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);
    if(pDirInfo->nFiles > 0)
    {
        CHL_HT_ITERATOR itr;
        if(FAILED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
        {
            logerr(L"Cannot iterate through file list for dir: %s", pDirInfo->pszPath);
        }
        else
        {
            char* pszKey;
            PFILEINFO pFileInfo;
            int nKeySize, nValSize;
            while(SUCCEEDED(CHL_DsGetNextHT(&itr, &pszKey, &nKeySize, &pFileInfo, &nValSize, TRUE)))
            {
                ClearDuplicateAttr(pFileInfo);
            }
        }
    }
}

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo)
{
    BOOL fRetVal = TRUE;

    WCHAR pszFilepath[MAX_PATH];
    loginfo(L"Deleting file = %s", pszFilepath);
    if(PathCombine(pszFilepath, pFileInfo->szPath, pFileInfo->szFilename) == NULL)
    {
        logerr(L"PathCombine() failed for %s + %s", pFileInfo->szPath, pFileInfo->szFilename);
        fRetVal = FALSE;
        goto done;
    }

    if(!DeleteFile(pszFilepath))
    {
        logerr(L"DeleteFile() failed.");
        fRetVal = FALSE;
    }

    // Remove from file list
    // The hashtable supports only char* keys
    char szKey[MAX_PATH];
    _ConvertToAscii(pFileInfo->szFilename, szKey);
    if(FAILED(CHL_DsRemoveHT(pDirInfo->phtFiles, szKey, strnlen_s(szKey, MAX_PATH) + 1)))
    {
        // See if file is present in the dup within list
        if(!(RemoveFromDupWithinList(pFileInfo, &pDirInfo->stDupFilesInTree)))
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

static BOOL _DeleteFileUpdateDir(_In_ PFILEINFO pFileToDelete, _In_ PDIRINFO pDeleteFrom, _In_opt_ PDIRINFO pUpdateDir)
{
    if(pUpdateDir)
    {
        char szKey[MAX_PATH];
        PFILEINFO pFileToUpdate;

        // Find this file in the other directory and update that file info
        // to say that it is not a duplicate any more.
        _ConvertToAscii(pFileToDelete->szFilename, szKey);
        BOOL fFound = SUCCEEDED(CHL_DsFindHT(pUpdateDir->phtFiles, szKey, strnlen_s(szKey, MAX_PATH) + 1,
            &pFileToUpdate, NULL, TRUE));
    
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
    if(pDirDeleteFrom->nFiles <= 0)
    {
        logwarn(L"Empty folder");
        goto error_return;
    }

    CHL_HT_ITERATOR itr;
    if(FAILED(CHL_DsInitIteratorHT(pDirDeleteFrom->phtFiles, &itr)))
    {
        goto error_return;
    }

    char* pszFilename = NULL;
    PFILEINFO pFileInfo = NULL;
    PFILEINFO pPrevDupFileInfo = NULL;
    while(SUCCEEDED(CHL_DsGetNextHT(&itr, &pszFilename, NULL, &pFileInfo, NULL, TRUE)))
    {
        // The hashtable iterator expects the currently found entry
        // not to be removed until we move to the next entry. Hence,
        // this logic with previous pointer.
        if(pPrevDupFileInfo)
        {
            _DeleteFileUpdateDir(pPrevDupFileInfo, pDirDeleteFrom, pDirToUpdate);
            pPrevDupFileInfo = NULL;
        }

        if(IsDuplicateFile(pFileInfo))
        {
            pPrevDupFileInfo = pFileInfo;
        }
    }

    if(pPrevDupFileInfo)
    {
        _DeleteFileUpdateDir(pPrevDupFileInfo, pDirDeleteFrom, pDirToUpdate);
        pPrevDupFileInfo = NULL;
    }

    return TRUE;

error_return:
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

    char szKey[MAX_PATH];
    int nKeySize;
    PFILEINFO pFileToDelete;
    //PFILEINFO pFileToUpdate;

    int index = 0;
    loginfo(L"Deleting %d files from %s", nFileNames, pDirDeleteFrom->pszPath);
    while(index < nFileNames)
    {
        _ConvertToAscii(paszFileNamesToDelete + (index * MAX_PATH), szKey);
        nKeySize = strnlen_s(szKey, ARRAYSIZE(szKey)) + 1;
        if(SUCCEEDED(CHL_DsFindHT(pDirDeleteFrom->phtFiles, szKey, nKeySize, &pFileToDelete, NULL, TRUE)))
        {
            _DeleteFileUpdateDir(pFileToDelete, pDirDeleteFrom, pDirToUpdate);
        }
        else
        {
            logwarn(L"Couldn't find file in dir");
        }

        ++index;
    }

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
    if(pRootDir->stDupFilesInTree.nCurFiles > 0)
    {
        wprintf(L"These are the files that are duplicate within the specified root folder's tree itself:\n");
        for(int i = 0; i < pRootDir->stDupFilesInTree.nCurFiles; ++i)
        {
            PFILEINFO pFileInfo = pRootDir->stDupFilesInTree.apFiles[i];
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
    if(FAILED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
    {
        logerr(L"CHL_DsInitIteratorHT() failed.");
        return;
    }

    wprintf(L"%s\n", pDirInfo->pszPath);

    char* pszFilename = NULL;
    PFILEINFO pFileInfo = NULL;
    while(SUCCEEDED(CHL_DsGetNextHT(&itr, &pszFilename, NULL, &pFileInfo, NULL, TRUE)))
    {
        wprintf(L"%10u %c %1d %s\n", 
            pFileInfo->llFilesize.LowPart,
            pFileInfo->fIsDirectory ? L'D' : L'F',
            IsDuplicateFile(pFileInfo) ? 1 : 0,
            pFileInfo->szFilename);
    }

    return;
}

static BOOL AddToDupWithinList(_In_ PDUPFILES_WITHIN pDupWithin, _In_ PFILEINFO pFileInfo)
{
    BOOL fContinue = TRUE;
    if(pDupWithin->nCurFiles >= pDupWithin->nCurSize)
    {
        // Must resize buffer

        if(CHL_GnIsOverflowINT(pDupWithin->nCurSize, pDupWithin->nCurSize))
        {
            logerr(L"Reached limit of int arithmetic for nCurSize = %d!", pDupWithin->nCurSize);
            fContinue = FALSE;
        }
        else
        {
            int nNewNumFiles = pDupWithin->nCurSize << 1;
            logdbg(L"Resizing to size %d", nNewNumFiles);
            PFILEINFO* apnew = (PFILEINFO*)realloc(pDupWithin->apFiles, sizeof(PFILEINFO) * nNewNumFiles);
            if(apnew != NULL)
            {
                pDupWithin->apFiles = apnew;
                pDupWithin->nCurSize = nNewNumFiles;
            }
            else
            {
                logerr(L"Out of memory.");
                fContinue = FALSE;
            }
        }
    }

    if(fContinue)
    {
        // Now, insert into list
        pDupWithin->apFiles[pDupWithin->nCurFiles] = pFileInfo;
        ++(pDupWithin->nCurFiles);
    }
    return fContinue;
}

static int FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ PDUPFILES_WITHIN pDupWithinToSearch, _In_ int iStartIndex)
{
    SB_ASSERT(iStartIndex >= 0 && iStartIndex <= pDupWithinToSearch->nCurFiles);
    for(int i = iStartIndex; i < pDupWithinToSearch->nCurFiles; ++i)
    {
        PFILEINFO pFile = pDupWithinToSearch->apFiles[i];
        if(pFile == NULL)
        {
            continue;
        }

        if(_wcsnicmp(pszFilename, pFile->szFilename, MAX_PATH) == 0)
        {
            return i;
        }
    }
    return -1;
}

static BOOL RemoveFromDupWithinList(_In_ PFILEINFO pFileToDelete, _In_ PDUPFILES_WITHIN pDupWithinToSearch)
{
    WCHAR pszCompareFromPath[MAX_PATH];
    if(PathCombine(pszCompareFromPath, pFileToDelete->szPath, pFileToDelete->szFilename) == NULL)
    {
        logerr(L"PathCombine() failed for %s + %s", pFileToDelete->szPath, pFileToDelete->szFilename);
        return FALSE;
    }

    WCHAR pszCompareToPath[MAX_PATH];
    for(int i = 0; i < pDupWithinToSearch->nCurFiles; ++i)
    {
        PFILEINFO pFile = pDupWithinToSearch->apFiles[i];
        if(PathCombine(pszCompareToPath, pFile->szPath, pFile->szFilename) == NULL)
        {
            logerr(L"PathCombine() failed for %s + %s", pFile->szPath, pFile->szFilename);
            return FALSE;
        }

        if(_wcsnicmp(pszCompareFromPath, pszCompareToPath, MAX_PATH) == 0)
        {
            free(pFile);
            pDupWithinToSearch->apFiles[i] = NULL;
            return TRUE;
        }
    }
    return FALSE;
}
