
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker.h"

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

static HRESULT _Init(_In_ PCWSTR pszFolderpath, _Out_ PDIRINFO* ppDirInfo)
{
    HRESULT hr = S_OK;
    PDIRINFO pDirInfo = (PDIRINFO)malloc(sizeof(DIRINFO));
    if(pDirInfo == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto error_return;
    }

    wcscpy_s(pDirInfo->pszPath, ARRAYSIZE(pDirInfo->pszPath), pszFolderpath);
    pDirInfo->nFiles = 0;

    // Create the hashtable for holding file information
    if(!fChlDsCreateHT(&pDirInfo->phtFiles, 256, HT_KEY_STR, HT_VAL_PTR, TRUE))
    {
        hr = E_FAIL;
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

    free(pDirInfo);
}

static BOOL _DeleteFile(_In_ PDIRINFO pDirInfo, _In_ PFILEINFO pFileInfo)
{
    BOOL fRetVal = TRUE;
    char szKey[MAX_PATH];

    WCHAR pszFilepath[MAX_PATH] = L"";
    wcscpy_s(pszFilepath, ARRAYSIZE(pszFilepath), pDirInfo->pszPath);

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

// Build the list of files in the given folder.
BOOL BuildFilesInDir(_In_ PCWSTR pszFolderpath, _Out_ PDIRINFO* ppDirInfo)
{
    //NT_ASSERT(pszFolderpath);
    //NT_ASSERT(ppDirInfo);

    if(FAILED(_Init(pszFolderpath, ppDirInfo)))
    {
        goto error_return;
    }

    // Derefernce just to make it easier to code
    PDIRINFO pDirInfo = *ppDirInfo;
    char szKey[MAX_PATH];

    WIN32_FIND_DATA findData;
    WCHAR szSearchpath[MAX_PATH] = L"";

    // In order to list all files within the specified directory,
    // path sent to FindFirstFile must end with a "\\*"
    wcscpy_s(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath);

    int nLen = wcsnlen(pszFolderpath, MAX_PATH);
    if(nLen > 2 && wcsncmp(pszFolderpath + nLen - 2, L"\\*", MAX_PATH) != 0)
    {
        wcscpy_s(szSearchpath, ARRAYSIZE(szSearchpath), pszFolderpath);
        wcscat_s(szSearchpath, ARRAYSIZE(szSearchpath), L"\\*");
    }

    // Loop through the file list and add files into hashtable
    HANDLE hFindFile = FindFirstFile(szSearchpath, &findData);
    if(hFindFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND)
    {
        // No files found under the folder. Just return.
        return TRUE;
    }
    else if(hFindFile == INVALID_HANDLE_VALUE)
    {
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

        // The hashtable supports only char* keys
        _ConvertToAscii(findData.cFileName, szKey);
        if(!fChlDsInsertHT(
                pDirInfo->phtFiles, 
                szKey, strnlen_s(szKey, MAX_PATH) + 1, 
                pFileInfo, sizeof(pFileInfo)))
        {
            logerr(L"Cannot add to file list: %s", szSearchpath);
            goto error_return;
        }

        ++(pDirInfo->nFiles);

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

    return TRUE;

error_return:
    return FALSE;
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

error_return:
    return FALSE;
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
