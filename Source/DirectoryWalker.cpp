
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker.h"

#define DDUP_NO_MATCH       0
#define DDUP_SOME_MATCH     1
#define DDUP_ALL_MATCH      2

#define INIT_DUP_WITHIN_SIZE    32

static WCHAR *g_apszBannedFilesFolders[] = 
{
    L".",
    L"..",
    L"$RECYCLE.BIN",
    L"System Volume Information",

    L"desktop.ini"
};

static void _ConvertToAscii(_In_ PCWSTR pwsz, _Out_ char* psz);
static BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars);
static BOOL AddToDupWithinList(_In_ DIRINFO::_dupWithin* pDupWithin, _In_ PFILEINFO pFileInfo);
static BOOL FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ DIRINFO::_dupWithin* pDupWithinToSearch, _Out_ PFILEINFO *ppFoundFile);

static HRESULT _Init(_In_ PCWSTR pszFolderpath, _In_opt_ const PDIRINFO pParentDir, _Out_ PDIRINFO* ppDirInfo)
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

    if(!fChlDsCreateHT(&pDirInfo->phtFiles, 256, HT_KEY_STR, HT_VAL_PTR, TRUE))
    {
        logerr(L"Couldn't create hash table for dir: %s", pszFolderpath);
        hr = E_FAIL;
        goto error_return;
    }

    // Allocate an array of pointer for the dup within files
    pDirInfo->stDupWithin.nCurSize = INIT_DUP_WITHIN_SIZE;
    pDirInfo->stDupWithin.apFiles = (PFILEINFO*)malloc(sizeof(PFILEINFO) * INIT_DUP_WITHIN_SIZE);
    if(pDirInfo->stDupWithin.apFiles == NULL)
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
        if(pDirInfo->phtFiles != NULL)
        {
            fChlDsDestroyHT(pDirInfo->phtFiles);
        }

        if(pDirInfo->stDupWithin.apFiles != NULL)
        {
            free(pDirInfo->stDupWithin.apFiles);
        }

        free(pDirInfo);
    }
    return hr;
}

void DestroyDirInfo(_In_ PDIRINFO pDirInfo)
{
    //NT_ASSERT(pDirInfo);

    if(pDirInfo->phtFiles != NULL)
    {
        fChlDsDestroyHT(pDirInfo->phtFiles);
    }

    if(pDirInfo->stDupWithin.apFiles != NULL)
    {
        free(pDirInfo->stDupWithin.apFiles);
    }

    free(pDirInfo);
}

// Build a tree 
BOOL BuildDirTree(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir)
{
    PCHL_QUEUE pqDirsToTraverse;
    if(FAILED(CHL_QueueCreate(&pqDirsToTraverse, CHL_VT_PVOID, 20)))
    {
        logerr(L"Could not create queue for BFS. Rootpath: %s", pszRootpath);
        goto error_return;
    }

    loginfo(L"Starting traversal for root dir: %s", pszRootpath);

    // Init the first dir to traverse. 
    // ** IMP: pFirst must be NULL here so that a new object is created in callee
    PDIRINFO pFirstDir = NULL;
    if(!BuildFilesInDir(pszRootpath, pqDirsToTraverse, &pFirstDir))
    {
        logerr(L"Could not build files in dir: %s", pszRootpath);
        goto error_return;
    }

    PDIRINFO pDirToTraverse;
    while(SUCCEEDED(pqDirsToTraverse->Delete(pqDirsToTraverse, (PVOID*)&pDirToTraverse)))
    {
        loginfo(L"Continuing traversal in dir: %s", pDirToTraverse->pszPath);
        if(!BuildFilesInDir(pDirToTraverse->pszPath, pqDirsToTraverse, &pFirstDir))
        {
            logerr(L"Could not build files in dir: %s", pszRootpath);
            goto error_return;
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
BOOL BuildFilesInDir(_In_ PCWSTR pszFolderpath, _In_opt_ PCHL_QUEUE pqDirsToTraverse, _Inout_ PDIRINFO* ppDirInfo)
{
    //NT_ASSERT(pszFolderpath);
    //NT_ASSERT(ppDirInfo);

    if(*ppDirInfo == NULL && FAILED(_Init(pszFolderpath, NULL, ppDirInfo)))
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
            logdbg(L"Banned: %s", findData.cFileName);
            continue;
        }

        szSearchpath[0] = 0;
        if(PathCombine(szSearchpath, pszFolderpath, findData.cFileName) == NULL)
        {
            logerr(L"PathCombine() failed for %s and %s", pszFolderpath, findData.cFileName);
            goto error_return;
        }

        PFILEINFO pFileInfo;
        if(!CreateFileInfo(szSearchpath, &pFileInfo))
        {
            // Treat as warning and move on.
            logwarn(L"Unable to get file info for: %s", szSearchpath);
            continue;
        }

        // If pqDirsToTraverse is not null, it means caller wants recursive directory traversal
        if(pqDirsToTraverse && pFileInfo->fIsDirectory)
        {
            PDIRINFO pSubDir;
            if(FAILED(_Init(szSearchpath, pCurDirInfo, &pSubDir)))
            {
                logwarn(L"Unable to init dir info for: %s", szSearchpath);
                free(pFileInfo);
                continue;
            }

            // Insert pSubDir into the queue so that it will be traversed later
            if(FAILED(pqDirsToTraverse->Insert(pqDirsToTraverse, pSubDir, sizeof *pSubDir)))
            {
                logwarn(L"Unable to add sub dir [%s] to traversal queue, cur dir: %s", findData.cFileName, pszFolderpath);
                free(pFileInfo);
                continue;
            }
            ++(pCurDirInfo->nDirs);
        }
        else
        {
            // The hashtable supports only char* keys
            char szKey[MAX_PATH];
            _ConvertToAscii(findData.cFileName, szKey);
            int nSize = strnlen_s(szKey, MAX_PATH) + 1;

            // If the current file's name is already inserted, then add it to the
            // dup within list
            if(fChlDsFindHT(pCurDirInfo->phtFiles, szKey, nSize, NULL, NULL))
            {
                AddToDupWithinList(&pCurDirInfo->stDupWithin, pFileInfo);
            }
            else
            {
                if(!fChlDsInsertHT(pCurDirInfo->phtFiles, szKey, nSize, pFileInfo, sizeof(pFileInfo)))
                {
                    logerr(L"Cannot add to file list: %s", szSearchpath);
                    free(pFileInfo);
                    goto error_return;
                }
            }

            ++(pCurDirInfo->nFiles);
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
        DestroyDirInfo(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return FALSE;
}

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir)
{
    //NT_ASSERT(pLeftDir);
    //NT_ASSERT(pRightDir);

    // Trivial check for directory to be empty
    if(pLeftDir->nFiles <= 0 || pRightDir->nFiles <= 0)
    {
        return TRUE;
    }

    // For each file in the left dir, search for the same in the right dir
    // If found, pass both to the CompareFileInfoAndMark().

    CHL_HT_ITERATOR itrLeft;
    if(!fChlDsInitIteratorHT(&itrLeft))
    {
        logerr(L"Cannot iterate through file list for dir: %s", pLeftDir->pszPath);
        goto error_return;
    }

    char* pszLeftFile;
    PFILEINFO pLeftFile, pRightFile;
    int nLeftKeySize, nLeftValSize, nRightValSize;

    logdbg(L"Comparing dirs: %s and %s", pLeftDir->pszPath, pRightDir->pszPath);
    while(fChlDsGetNextHT(pLeftDir->phtFiles, &itrLeft, &pszLeftFile, &nLeftKeySize, &pLeftFile, &nLeftValSize))
    {
        if(fChlDsFindHT(pRightDir->phtFiles, (void*)pszLeftFile, nLeftKeySize, &pRightFile, &nRightValSize))
        {
            // Same file found in right dir, compare and mark as duplicate
            if(CompareFileInfoAndMark(pLeftFile, pRightFile))
            {
                logdbg(L"Duplicate file: %S", pszLeftFile);
            }
        }
    }

    // See if dup within files are duplicate
    for(int i = 0; i < pLeftDir->stDupWithin.nCurFiles; ++i)
    {
        pLeftFile = pLeftDir->stDupWithin.apFiles[i];
        if(FindInDupWithinList(pLeftFile->szFilename, &pRightDir->stDupWithin, &pRightFile))
        {
            // Same file found in right dir, compare and mark as duplicate
            if(CompareFileInfoAndMark(pLeftFile, pRightFile))
            {
                logdbg(L"DupWithin Duplicate file: %S", pszLeftFile);
            }
        }
    }

    return TRUE;

error_return:
    return FALSE;
}

#pragma region FileOperations

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo)
{
    BOOL fRetVal = TRUE;
    char szKey[MAX_PATH];

    WCHAR pszFilepath[MAX_PATH] = L"";
    wcscpy_s(pszFilepath, ARRAYSIZE(pszFilepath), pFileInfo->szPath);

    loginfo(L"Deleting file = %s", pFileInfo->szFilename);
    wcscat_s(pszFilepath, ARRAYSIZE(pszFilepath), L"\\");
    wcscat_s(pszFilepath, ARRAYSIZE(pszFilepath), pFileInfo->szFilename);
    if(!DeleteFile(pszFilepath))
    {
        logerr(L"DeleteFile() failed.");
        fRetVal = FALSE;
    }
    else
    {   
        // Remove from file list
        // The hashtable supports only char* keys
        _ConvertToAscii(pFileInfo->szFilename, szKey);
        if(!fChlDsRemoveHT(pDirInfo->phtFiles, szKey, strnlen_s(szKey, MAX_PATH) + 1))
        {
            logwarn(L"Failed to remove file from list: %s", pFileInfo->szFilename);
            fRetVal = FALSE;
        }

        --(pDirInfo->nFiles);
    }

    return fRetVal;
}

void ClearFilesDupFlag(_In_ PDIRINFO pDirInfo)
{
    //NT_ASSERT(pDirInfo);

    if(pDirInfo->nFiles > 0)
    {
        CHL_HT_ITERATOR itr;
        if(!fChlDsInitIteratorHT(&itr))
        {
            logerr(L"Cannot iterate through file list for dir: %s", pDirInfo->pszPath);
        }
        else
        {
            char* pszKey;
            PFILEINFO pFileInfo;
            int nKeySize, nValSize;
            while(fChlDsGetNextHT(pDirInfo->phtFiles, &itr, &pszKey, &nKeySize, &pFileInfo, &nValSize))
            {
                ClearDuplicateAttr(pFileInfo);
            }
        }
    }
}

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate)
{
    //NT_ASSERT(pDirDeleteFrom)

    loginfo(L"In folder = %s", pDirDeleteFrom->pszPath);
    if(pDirDeleteFrom->nFiles <= 0)
    {
        logwarn(L"Empty folder");
        goto error_return;
    }

    CHL_HT_ITERATOR itr;
    if(!fChlDsInitIteratorHT(&itr))
    {
        goto error_return;
    }

    int nKeySize, nValSize;
    char* pszFilename = NULL;
    PFILEINFO pFileInfo = NULL;
    PFILEINFO pPrevDupFileInfo = NULL;
    while(fChlDsGetNextHT(pDirDeleteFrom->phtFiles, &itr, &pszFilename, &nKeySize, &pFileInfo, &nValSize))
    {
        //NT_ASSERT(nKeySize == sizeof(PWCHAR));
        //NT_ASSERT(nValSize == sizeof(PFILEINFO));

        // The hashtable iterator expects the currently found entry
        // not to be removed until we move to the next entry. Hence,
        // this logic with previous pointer.
        if(pPrevDupFileInfo)
        {
            if(pDirToUpdate)
            {
                // Find this file in the other directory and update that file info
                // to say that it is not a duplicate any more.
                char szKey[MAX_PATH];
                PFILEINFO pFileToUpdate;
                int nValSize;

                _ConvertToAscii(pPrevDupFileInfo->szFilename, szKey);
                BOOL fFound = fChlDsFindHT(pDirToUpdate->phtFiles, szKey, strnlen_s(szKey, MAX_PATH) + 1,
                                &pFileToUpdate, &nValSize);
                //NT_ASSERT(fFound);    // Must find in the other dir also.
                ClearDuplicateAttr(pFileToUpdate);
            }

            // Now, delete file from the specified dir
            _DeleteFile(pDirDeleteFrom, pPrevDupFileInfo);
        }

        if(IsDuplicateFile(pFileInfo))
        {
            pPrevDupFileInfo = pFileInfo;
        }
        else
        {
            pPrevDupFileInfo = NULL;
        }
    }

    if(pPrevDupFileInfo)
    {
        _DeleteFile(pDirDeleteFrom, pPrevDupFileInfo);
    }
    return TRUE;

error_return:
    return FALSE;
}

// Similar to the DeleteDupFilesInDir() function but it deletes only the specified files
// from the pDirDeleteFrom directory and update the other directory files'. The files to
// be deleted are specified in a list of strings that are the file names.
// ** Assuming that paszFileNamesToDelete is an array of MAX_PATH strings.
BOOL DeleteFilesInDir(
    _In_ PDIRINFO pDirDeleteFrom, 
    _In_ PCWSTR paszFileNamesToDelete, 
    _In_ int nFileNames, 
    _In_ PDIRINFO pDirToUpdate)
{
    //NT_ASSERT(pDirDeleteFrom);
    //NT_ASSERT(apszFileNamesToDelete);
    //NT_ASSERT(pDirToUpdate);
    //NT_ASSERT(nFileNames >= 0);

    char szKey[MAX_PATH];
    int nKeySize;
    PFILEINFO pFileToDelete;
    PFILEINFO pFileToUpdate;
    int nValSize;

    int index = 0;
    loginfo(L"Deleting %d files from %s", nFileNames, pDirDeleteFrom->pszPath);
    while(index < nFileNames)
    {
        _ConvertToAscii(paszFileNamesToDelete + (index * MAX_PATH), szKey);
        nKeySize = strnlen_s(szKey, ARRAYSIZE(szKey)) + 1;
        if(fChlDsFindHT(pDirDeleteFrom->phtFiles, szKey, nKeySize, &pFileToDelete, &nValSize))
        {
            // File found in from-dir
            // If file is present in to-update-dir, then clear it's duplicate flag
            // TODO: Better logic; don't check this inside the loop, every iteration
            if(pDirToUpdate)
            {
                if(fChlDsFindHT(pDirToUpdate->phtFiles, szKey, nKeySize, &pFileToUpdate, &nValSize))
                {
                    // Don't care what kind of duplicacy it was, now there will be no duplicate.
                    ClearDuplicateAttr(pFileToUpdate);
                }
            }

            // Delete file from file system and remove from hashtable
            _DeleteFile(pDirDeleteFrom, pFileToDelete);
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
void PrintDirTree(_In_ PDIRINFO pRootDir)
{
    PrintFilesInDir(pRootDir);
    wprintf(L"\n");

    // Print any dup within files
    if(pRootDir->stDupWithin.nCurFiles > 0)
    {
        wprintf(L"These are the files that are duplicate within the specified root folder's tree itself:\n");
        for(int i = 0; i < pRootDir->stDupWithin.nCurFiles; ++i)
        {
            PFILEINFO pFileInfo = pRootDir->stDupWithin.apFiles[i];
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
void PrintFilesInDir(_In_ PDIRINFO pDirInfo)
{
    //NT_ASSERT(pDirInfo)

    CHL_HT_ITERATOR itr;
    if(!fChlDsInitIteratorHT(&itr))
    {
        logerr(L"fChlDsInitIteratorHT() failed.");
        return;
    }

    wprintf(L"%s\n", pDirInfo->pszPath);

    int nKeySize, nValSize;
    char* pszFilename = NULL;
    PFILEINFO pFileInfo = NULL;
    while(fChlDsGetNextHT(pDirInfo->phtFiles, &itr, &pszFilename, &nKeySize, &pFileInfo, &nValSize))
    {
        //NT_ASSERT(nKeySize == sizeof(PWCHAR));
        //NT_ASSERT(nValSize == sizeof(PFILEINFO));

        wprintf(L"%10u %c %1d %s\n", 
            pFileInfo->llFilesize.LowPart,
            pFileInfo->fIsDirectory ? L'D' : L'F',
            IsDuplicateFile(pFileInfo) ? 1 : 0,
            pFileInfo->szFilename);
    }

    return;
}

// Temp function to convert. Assume safe null-terminated strings
// with appropriate sized buffers allocated.
static void _ConvertToAscii(_In_ PCWSTR pwsz, _Out_ char* psz)
{
    size_t nOrig = wcslen(pwsz) + 1;
    wcstombs(psz, pwsz, nOrig);
}

static BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars)
{
    for(int i = 0; i < ARRAYSIZE(g_apszBannedFilesFolders); ++i)
    {
        if(_wcsnicmp(pszFilename, g_apszBannedFilesFolders[i], nMaxChars) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL AddToDupWithinList(_In_ DIRINFO::_dupWithin* pDupWithin, _In_ PFILEINFO pFileInfo)
{
    if(pDupWithin->nCurFiles < pDupWithin->nCurSize)
    {
        pDupWithin->apFiles[pDupWithin->nCurFiles] = pFileInfo;
        ++(pDupWithin->nCurFiles);
        return TRUE;
    }

    // TODO: Realloc if size is insufficient
    return FALSE;
}

static BOOL FindInDupWithinList(_In_ PCWSTR pszFilename, _In_ DIRINFO::_dupWithin* pDupWithinToSearch, _Out_ PFILEINFO *ppFoundFile)
{
    for(int i = 0; i < pDupWithinToSearch->nCurFiles; ++i)
    {
        PFILEINFO pFile = pDupWithinToSearch->apFiles[i];
        if(_wcsnicmp(pszFilename, pFile->szFilename, MAX_PATH) == 0)
        {
            *ppFoundFile = pFile;
            return TRUE;
        }
    }
    return FALSE;
}
