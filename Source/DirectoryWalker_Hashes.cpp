
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker_Hashes.h"
#include "DirectoryWalker_Util.h"
#include "HashFactory.h"

static BOOL InsertIntoFileList(_In_ PDIRINFO pDirInfo, _In_opt_ PCWSTR pszKey, _In_ PFILEINFO pFile);

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

    // Last parameter is FALSE indicating that the value is not be free'd by the hashtable
    // library upon item removal/hashtable destruction. We will do it ourselves.
    int nEstEntries = fRecursive ? 2048 : 256;
    if (FAILED(CHL_DsCreateHT(&pDirInfo->phtFiles, nEstEntries, CHL_KT_STRING, CHL_VT_POINTER, FALSE)))
    {
        logerr(L"Couldn't create hash table for dir: %s", pszFolderpath);
        hr = E_FAIL;
        goto error_return;
    }

    pDirInfo->fHashCompare = TRUE;

    *ppDirInfo = pDirInfo;
    return hr;

error_return:
    *ppDirInfo = NULL;
    if (pDirInfo != NULL)
    {
        DestroyDirInfo_Hash(pDirInfo);
    }
    return hr;
}

void DestroyDirInfo_Hash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    if (pDirInfo->phtFiles != NULL)
    {
        // First, destroy all linked lists
        char* pszHash;
        int nKeySize;
        CHL_HT_ITERATOR itr;
        PCHL_LLIST pList;

        if (SUCCEEDED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
        {
            while (SUCCEEDED(itr.GetCurrent(&itr, &pszHash, &nKeySize, &pList, NULL, TRUE)))
            {
                (void)itr.MoveNext(&itr);
                SB_ASSERT(pList);
                CHL_DsDestroyLL(pList);
            }
        }

        // Finally, the hash table itself
        CHL_DsDestroyHT(pDirInfo->phtFiles);
    }

    // Finally finally, the DIRINFO itself
    free(pDirInfo);
}

// Build a tree 
BOOL BuildDirTree_Hash(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir)
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
    if (!BuildFilesInDir_Hash(pszRootpath, pqDirsToTraverse, &pFirstDir))
    {
        logerr(L"Could not build files in dir: %s", pszRootpath);
        goto error_return;
    }

    PDIRINFO pDirToTraverse;
    while (SUCCEEDED(pqDirsToTraverse->Delete(pqDirsToTraverse, &pDirToTraverse, NULL, FALSE)))
    {
        loginfo(L"Continuing traversal in dir: %s", pDirToTraverse->pszPath);
        if (!BuildFilesInDir_Hash(pDirToTraverse->pszPath, pqDirsToTraverse, &pFirstDir))
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
BOOL BuildFilesInDir_Hash(
    _In_ PCWSTR pszFolderpath,
    _In_opt_ PCHL_QUEUE pqDirsToTraverse,
    _Inout_ PDIRINFO* ppDirInfo)
{
    SB_ASSERT(pszFolderpath);
    SB_ASSERT(ppDirInfo);

    WIN32_FIND_DATA findData;
    HANDLE hFindFile = INVALID_HANDLE_VALUE;
    PDIRINFO pCurDirInfo = NULL;

    if (*ppDirInfo == NULL && FAILED(_Init(pszFolderpath, (pqDirsToTraverse != NULL), ppDirInfo)))
    {
        logerr(L"Init failed for dir: %s", pszFolderpath);
        goto error_return;
    }

    // Derefernce just to make it easier to code
    pCurDirInfo = *ppDirInfo;

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
        if (!CreateFileInfo(szSearchpath, TRUE, &pFileInfo))
        {
            // Treat as warning and move on.
            logwarn(L"Unable to get file info for: %s", szSearchpath);
            continue;
        }

        if (pFileInfo->fIsDirectory)
        {
            // If pqDirsToTraverse is not null, it means caller wants recursive directory traversal
            if (pqDirsToTraverse)
            {
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
                // Ignore directories when recursive mode is turned OFF
                free(pFileInfo);
                logdbg(L"Skipped adding dir: %s", findData.cFileName);
            }
        }
        else
        {
            if (!InsertIntoFileList(pCurDirInfo, findData.cFileName, pFileInfo))
            {
                logerr(L"Cannot add file to file list: %s", findData.cFileName);
                free(pFileInfo);
            }
#ifdef _DEBUG
            else
            {
                logdbg(L"Added file: %s", findData.cFileName);
            }
#endif
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
        DestroyDirInfo_Hash(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return FALSE;
}

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles_Hash(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir)
{
    // For each file in the left dir, search for the same in the right dir
    // If found, pass both to the CompareFileInfoAndMark().

    CHL_HT_ITERATOR itrLeft;
    if (FAILED(CHL_DsInitIteratorHT(pLeftDir->phtFiles, &itrLeft)))
    {
        return TRUE;
    }

    char* pszLeftHash;
    int nLeftKeySize;

    PCHL_LLIST pLeftList, pRightList;

    logdbg(L"Comparing dirs: %s and %s", pLeftDir->pszPath, pRightDir->pszPath);
    while (SUCCEEDED(itrLeft.GetCurrent(&itrLeft, &pszLeftHash, &nLeftKeySize, &pLeftList, NULL, TRUE)))
    {
        (void)itrLeft.MoveNext(&itrLeft);

        PCHL_LLIST pTemp;   // TODO: why is this here?
        CHL_DsFindHT(pLeftDir->phtFiles, pszLeftHash, nLeftKeySize, &pTemp, NULL, TRUE);

        if (SUCCEEDED(CHL_DsFindHT(pRightDir->phtFiles, pszLeftHash, nLeftKeySize, &pRightList, NULL, TRUE)))
        {
            // Not supporting duplicate files in same directory scenario

            PFILEINFO pLeftFile, pRightFile;
            if (FAILED(CHL_DsPeekAtLL(pLeftList, 0, &pLeftFile, NULL, TRUE)))
            {
                SB_ASSERT(FALSE);
            }

            if (FAILED(CHL_DsPeekAtLL(pRightList, 0, &pRightFile, NULL, TRUE)))
            {
                SB_ASSERT(FALSE);
            }

#ifdef _DEBUG
            if (CompareFileInfoAndMark(pLeftFile, pRightFile, TRUE))
            {
                loginfo(L"Hash duplicate files: %s, %s", pLeftFile->szFilename, pRightFile->szFilename);
            }
            else
            {
                // Must be duplicate files since their hash values match
                SB_ASSERT(FALSE);
            }
#else
            CompareFileInfoAndMark(pLeftFile, pRightFile, TRUE);
#endif
        }
    }

    return TRUE;
}

#pragma region FileOperations

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo)
{
    SB_ASSERT(pFileInfo->fIsDirectory == FALSE);

    BOOL fRetVal = TRUE;

    WCHAR szFilepath[MAX_PATH];
    if (FAILED(PathCchCombine(szFilepath, ARRAYSIZE(szFilepath), pFileInfo->szPath, pFileInfo->szFilename) == NULL))
    {
        logerr(L"PathCchCombine() failed for %s + %s", pFileInfo->szPath, pFileInfo->szFilename);
        fRetVal = FALSE;
        goto done;
    }

    loginfo(L"Deleting file = %s", szFilepath);
    if (!DeleteFile(szFilepath))
    {
        logerr(L"DeleteFile failed, err: %u", GetLastError());
        fRetVal = FALSE;
    }

done:
    return fRetVal;
}

void ClearFilesDupFlag_Hash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    if (pDirInfo->nFiles > 0)
    {
        CHL_HT_ITERATOR itr;
        if (SUCCEEDED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
        {
            char* pszKey;
            PCHL_LLIST pList;
            while (SUCCEEDED(itr.GetCurrent(&itr, &pszKey, NULL, &pList, NULL, TRUE)))
            {
                (void)itr.MoveNext(&itr);

                // Foreach file in the linked list...
                PFILEINFO pFileInfo;
                for (int i = 0; i < pList->nCurNodes; ++i)
                {
                    if (FAILED(CHL_DsPeekAtLL(pList, i, &pFileInfo, NULL, TRUE)))
                    {
                        logerr(L"Cannot get item %d for hash string %S", i, pszKey);
                        continue;
                    }

                    ClearDuplicateAttr(pFileInfo);
                }
            }
        }
    }
}

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir_Hash(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate)
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

    char* pszKey;
    PCHL_LLIST pList = NULL;
    while (SUCCEEDED(itr.GetCurrent(&itr, &pszKey, NULL, &pList, NULL, TRUE)))
    {
        // Foreach file in the linked list...
        PFILEINFO pFileInfo;
        for (int i = 0; i < pList->nCurNodes; ++i)
        {
            if (FAILED(CHL_DsPeekAtLL(pList, i, &pFileInfo, NULL, TRUE)))
            {
                logerr(L"Cannot get item %d for hash string %S", i, pszKey);
                continue;
            }

            DelEmptyFolders_Add(phtFoldersSeen, pFileInfo);

            if ((pFileInfo->fIsDirectory == FALSE) && (IsDuplicateFile(pFileInfo) == TRUE))
            {
                _DeleteFile(pDirDeleteFrom, pFileInfo);

                // Linked list frees up memory when third param is NULL
                if (FAILED(CHL_DsRemoveAtLL(pList, i, NULL, NULL, TRUE)))
                {
                    logerr(L"Cannot remove file %s from linked list", pFileInfo->szFilename);
                }
                else
                {
                    // When we remove a node from linked list, next iteration will
                    // start from the current index itself.
                    --i;
                }
            }
        }

        if (pList->nCurNodes == 0)
        {
            CHL_DsDestroyLL(pList);
            
            if (pDirToUpdate) // BUG _In_ param, why check for NULL here?
            {
                // Find this file in the other directory and update that file info
                // to say that it is not a duplicate any more.
                PCHL_LLIST pRightList;
                PFILEINFO pFileToUpdate;

                BOOL fFound = SUCCEEDED(CHL_DsFindHT(pDirToUpdate->phtFiles, pszKey, strnlen_s(pszKey, MAX_PATH) + 1,
                    &pRightList, NULL, TRUE));
                SB_ASSERT(fFound);    // Must find in the other dir also.

                fFound = SUCCEEDED(CHL_DsPeekAtLL(pRightList, 0, &pFileToUpdate, NULL, TRUE));
                SB_ASSERT(fFound);

                ClearDuplicateAttr(pFileToUpdate);
            }

            if (FAILED(CHL_DsRemoveAtHT(&itr)))
            {
                logerr(L"Cannot remove key %s from hashtable", pszKey);
                SB_ASSERT(FALSE);
            }
        }
        else
        {
            (void)itr.MoveNext(&itr);
        }
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
BOOL DeleteFilesInDir_Hash(
    _In_ PDIRINFO pDirDeleteFrom,
    _In_ PFILEINFO *paFilesToDelete,
    _In_ int nFiles,
    _In_opt_ PDIRINFO pDirToUpdate)
{
    SB_ASSERT(pDirDeleteFrom);
    SB_ASSERT(paFilesToDelete);
    SB_ASSERT(nFiles >= 0);

    char szKey[STRLEN_SHA1];
    int nKeySize;
    PFILEINFO pFileToDelete;

    PCHL_HTABLE phtFoldersSeen;
    if (FAILED(DelEmptyFolders_Init(pDirDeleteFrom, &phtFoldersSeen)))
    {
        return FALSE;
    }

    int index = 0;
    loginfo(L"Deleting %d files from %s", nFiles, pDirDeleteFrom->pszPath);
    while (index < nFiles)
    {
        pFileToDelete = paFilesToDelete[index++];
        DelEmptyFolders_Add(phtFoldersSeen, pFileToDelete);
        if (pFileToDelete->fIsDirectory == TRUE)
        {
            continue;
        }

        HashValueToString(pFileToDelete->abHash, szKey);
        nKeySize = strnlen_s(szKey, ARRAYSIZE(szKey)) + 1;

        PCHL_LLIST pLeftList;
        if (SUCCEEDED(CHL_DsFindHT(pDirDeleteFrom->phtFiles, szKey, nKeySize, &pLeftList, NULL, TRUE)))
        {
            // Find the file in the linked list
            if (FAILED(CHL_DsFindLL(pLeftList, pFileToDelete, CompareFilesByName, NULL, NULL, TRUE)))
            {
                logerr(L"File to delete %s not found under hash string: %S", pFileToDelete->szFilename, szKey);
                SB_ASSERT(FALSE);
            }
            else
            {
                // File found in from-dir
                // If file is present in to-update-dir, then clear it's duplicate flag
                // TODO: Better logic; don't check this inside the loop, every iteration
                if (pDirToUpdate && (pFileToDelete->fIsDirectory == FALSE))
                {
                    PCHL_LLIST pRightList;
                    if (SUCCEEDED(CHL_DsFindHT(pDirToUpdate->phtFiles, szKey, nKeySize, &pRightList, NULL, TRUE)))
                    {
                        // Find the file in the linked list
                        if (SUCCEEDED(CHL_DsFindLL(pRightList, pFileToDelete, CompareFilesByName, NULL, NULL, TRUE)))
                        {
                            // Don't care what kind of duplicacy it was, now there will be no duplicate.
                            ClearDuplicateAttr(pFileToDelete);
                        }
                    }
                }
            }

            // Delete file from file system and remove from linkedlist (and hashtable)
            if (_DeleteFile(pDirDeleteFrom, pFileToDelete) == TRUE)
            {
                --(pLeftList->nCurNodes);

                if (pLeftList->nCurNodes == 0)
                {
                    CHL_DsDestroyLL(pLeftList);
                    CHL_DsRemoveHT(pDirDeleteFrom->phtFiles, szKey, nKeySize);
                }
            }
        }
        else
        {
            logwarn(L"Couldn't find file in dir");
        }
    }

    DelEmptyFolders_Delete(phtFoldersSeen);
    return TRUE;
}

#pragma endregion FileOperations

// Print the dir tree
void PrintDirTree_Hash(_In_ PDIRINFO pRootDir)
{
    PrintFilesInDir(pRootDir);
    return;
}

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir_Hash(_In_ PDIRINFO pDirInfo)
{
    SB_ASSERT(pDirInfo);

    CHL_HT_ITERATOR itr;
    if (FAILED(CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr)))
    {
        return;
    }

    wprintf(L"%s\n", pDirInfo->pszPath);

    char* pszKey = NULL;
    PCHL_LLIST pList = NULL;
    while (SUCCEEDED(itr.GetCurrent(&itr, &pszKey, NULL, &pList, NULL, TRUE)))
    {
        (void)itr.MoveNext(&itr);

        // Foreach file in the linked list...
        PFILEINFO pFileInfo;
        for (int i = 0; i < pList->nCurNodes; ++i)
        {
            if (FAILED(CHL_DsPeekAtLL(pList, i, &pFileInfo, NULL, TRUE)))
            {
                logerr(L"Cannot get item %d for hash string %S", i, pszKey);
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

static BOOL InsertIntoFileList(_In_ PDIRINFO pDirInfo, _In_opt_ PCWSTR pszKey, _In_ PFILEINFO pFile)
{
    // Key to the hash table is the file's hash string
    CHAR szHash[STRLEN_SHA1];
    HashValueToString(pFile->abHash, szHash);

    PCHL_LLIST pList = NULL;
    if (FAILED(CHL_DsFindHT(pDirInfo->phtFiles, szHash, STRLEN_SHA1, &pList, NULL, FALSE)))
    {
        // There was no prior file with the same hash value, so create a new linked list
        // for this hash value to store all files which have same hash value.
        if (FAILED(CHL_DsCreateLL(&pList, CHL_VT_POINTER, 5)))
        {
            logerr(L"Cannot create linked list for PFILEINFO storage in hashtable.");
            goto error_return;
        }

        // Insert hash value into table
        if (FAILED(CHL_DsInsertHT(pDirInfo->phtFiles, szHash, ARRAYSIZE(szHash), pList, sizeof pList)))
        {
            logerr(L"Cannot insert linked list for PFILEINFO storage in hashtable.");
            CHL_DsDestroyLL(pList);
            goto error_return;
        }
    }

    SB_ASSERT(pFile);
    if (FAILED(CHL_DsInsertLL(pList, pFile, sizeof pFile)))
    {
        logerr(L"Cannot insert into linked list: %s", pszKey);
        goto error_return;
    }

    ++(pDirInfo->nFiles);
    return TRUE;

error_return:
    free(pFile);
    return FALSE;
}
