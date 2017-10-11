
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "FileInfo.h"

static HCRYPTPROV g_hCrypt = NULL;

HRESULT FileInfoInit(_In_ BOOL fComputeHash)
{
    HRESULT hr = S_OK;
    if (fComputeHash && g_hCrypt == NULL)
    {
        hr = HashFactoryInit(&g_hCrypt);
    }
    return hr;
}

void FileInfoDestroy()
{
    if (g_hCrypt)
    {
        HashFactoryDestroy(g_hCrypt);
        g_hCrypt = NULL;
    }
}

// Populate file info for the specified file in the caller specified memory location
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _In_ BOOL fComputeHash, _In_ PFILEINFO pFileInfo)
{
    SB_ASSERT(pszFullpathToFile);
    SB_ASSERT(pFileInfo);

    ZeroMemory(pFileInfo, sizeof(*pFileInfo));

    // Copy file/dir name over to the FILEINFO struct
    PCWSTR pszFilename = CHL_SzGetFilenameFromPath(pszFullpathToFile, wcslen(pszFullpathToFile));
    int nCharsInRoot = 0;
    for (const WCHAR* pch = pszFullpathToFile; pch != pszFilename; ++pch, ++nCharsInRoot)
        ;

    // Copy folder path first (we've calculate the length to copy)
    wcsncpy_s(pFileInfo->szPath, ARRAYSIZE(pFileInfo->szPath), pszFullpathToFile, nCharsInRoot);

    // Now copy the filename
    wcscpy_s(pFileInfo->szFilename, ARRAYSIZE(pFileInfo->szFilename), pszFilename);

    // Check if file is a directory
    // TODO: To extend this limit to 32,767 wide characters, 
    // call the Unicode version of the function and prepend "\\?\" to the path
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    if (!GetFileAttributesEx(pszFullpathToFile, GetFileExInfoStandard, &fileAttr))
    {
        logerr(L"Unable to get attributes of file: %s", pszFullpathToFile);
        goto error_return;
    }

    // Store FILETIME in fileinfo for easy comparison in sorting the listview rows
    CopyMemory(&pFileInfo->ftModifiedTime, &fileAttr.ftLastWriteTime, sizeof(pFileInfo->ftModifiedTime));

    // Convert filetime to localtime and store in fileinfo
    SYSTEMTIME stUTC;
    FileTimeToSystemTime(&pFileInfo->ftModifiedTime, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &pFileInfo->stModifiedTime);

    if (fileAttr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        pFileInfo->fIsDirectory = TRUE;
    }
    else
    {
        pFileInfo->llFilesize.HighPart = fileAttr.nFileSizeHigh;
        pFileInfo->llFilesize.LowPart = fileAttr.nFileSizeLow;

        if (fComputeHash)
        {
            // Generate hash. Open file handle first...    
            HANDLE hFile = CreateFileW(pszFullpathToFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                logerr(L"Failed to open file %s", pszFullpathToFile);
                goto error_return;
            }

            HRESULT hr = CalculateSHA1(g_hCrypt, hFile, pFileInfo->abHash);
            CloseHandle(hFile);
            if (FAILED(hr))
            {
                logerr(L"Failed to compute hash (0x%08x) for file: %s", hr, pszFullpathToFile);
                goto error_return;
            }
        }
    }

    return TRUE;

error_return:
    return FALSE;
}

// Populate file info for the specified file in the callee heap-allocated memory location
// and return the pointer to this location to the caller.
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _In_ BOOL fComputeHash, _Out_ PFILEINFO* ppFileInfo)
{
    SB_ASSERT(pszFullpathToFile);
    SB_ASSERT(ppFileInfo);

    PFILEINFO pFileInfo;
    pFileInfo = (PFILEINFO)malloc(sizeof(FILEINFO));
    if (pFileInfo == NULL)
    {
        goto error_return;
    }

    if (!CreateFileInfo(pszFullpathToFile, fComputeHash, pFileInfo))
    {
        goto error_return;
    }

    *ppFileInfo = pFileInfo;
    return TRUE;

error_return:
    if (pFileInfo != NULL)
    {
        free(pFileInfo);
    }

    *ppFileInfo = NULL;
    return FALSE;
}

// Compare two file info structs and say whether they are equal or not
// also set duplicate flag in the file info structs.
BOOL CompareFileInfoAndMark(_In_ const PFILEINFO pLeftFile, _In_ const PFILEINFO pRightFile, _In_ BOOL fCompareHashes)
{
    SB_ASSERT(pLeftFile);
    SB_ASSERT(pRightFile);

    // Along with setting bits, must also clear bits 
    // in case of no match because:
    // User could have chosen two folders first and then
    // changed one of the folders which would invaliate
    // the unchanged folder's files' duplicacy.

    // Two directories match only if their names are the same
    if (pLeftFile->fIsDirectory && pRightFile->fIsDirectory)
    {
        if (_wcsnicmp(pLeftFile->szFilename, pRightFile->szFilename, MAX_PATH) == 0)
        {
            pLeftFile->bDupInfo |= FDUP_NAME_MATCH;
            pRightFile->bDupInfo |= FDUP_NAME_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_NAME_MATCH);
            pRightFile->bDupInfo &= (~FDUP_NAME_MATCH);
        }
    }
    else if (pLeftFile->fIsDirectory || pRightFile->fIsDirectory)
    {
        // Only one of them is a directory, there can be no match except a
        // name match between a file and directory but it isn't very useful.
        pLeftFile->bDupInfo = FDUP_NO_MATCH;
        pRightFile->bDupInfo = FDUP_NO_MATCH;
    }
    else
    {
        // Both are files, perform the various match checks

        if (_wcsnicmp(pLeftFile->szFilename, pRightFile->szFilename, MAX_PATH) == 0)
        {
            pLeftFile->bDupInfo |= FDUP_NAME_MATCH;
            pRightFile->bDupInfo |= FDUP_NAME_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_NAME_MATCH);
            pRightFile->bDupInfo &= (~FDUP_NAME_MATCH);
        }

        if (pLeftFile->llFilesize.QuadPart == pRightFile->llFilesize.QuadPart)
        {
            pLeftFile->bDupInfo |= FDUP_SIZE_MATCH;
            pRightFile->bDupInfo |= FDUP_SIZE_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_SIZE_MATCH);
            pRightFile->bDupInfo &= (~FDUP_SIZE_MATCH);
        }

        if (memcmp(&pLeftFile->stModifiedTime, &pRightFile->stModifiedTime, sizeof(pLeftFile->stModifiedTime)) == 0)
        {
            pLeftFile->bDupInfo |= FDUP_DATE_MATCH;
            pRightFile->bDupInfo |= FDUP_DATE_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_DATE_MATCH);
            pRightFile->bDupInfo &= (~FDUP_DATE_MATCH);
        }

        if (fCompareHashes)
        {
            if (memcmp(&pLeftFile->abHash, &pRightFile->abHash, sizeof(pLeftFile->abHash)) == 0)
            {
                pLeftFile->bDupInfo |= FDUP_HASH_MATCH;
                pRightFile->bDupInfo |= FDUP_HASH_MATCH;
            }
            else
            {
                pLeftFile->bDupInfo &= (~FDUP_HASH_MATCH);
                pRightFile->bDupInfo &= (~FDUP_HASH_MATCH);
            }
        }
    }

    return IsDuplicateFile(pLeftFile);
}

inline BOOL IsDuplicateFile(_In_ const PFILEINFO pFileInfo)
{
    BYTE bDupInfo = pFileInfo->bDupInfo;
    if (pFileInfo->fIsDirectory)
    {
        return (bDupInfo & FDUP_NAME_MATCH);
    }

    return (bDupInfo & FDUP_HASH_MATCH) ||
        ((bDupInfo & FDUP_NAME_MATCH) &&
        (bDupInfo & FDUP_SIZE_MATCH) &&
            (bDupInfo & FDUP_DATE_MATCH));
}

void GetDupTypeString(_In_ PFILEINFO pFileInfo, _Inout_z_ PWSTR pszDupType)
{
    PWCHAR pch = pszDupType;

    *pch = 0;

    BYTE bDupInfo = pFileInfo->bDupInfo;
    if (bDupInfo != FDUP_NO_MATCH)
    {
        if (bDupInfo & FDUP_NAME_MATCH)
        {
            *pch++ = L'N';
            *pch++ = L',';
        }

        if (bDupInfo & FDUP_SIZE_MATCH)
        {
            *pch++ = L'S';
            *pch++ = L',';
        }

        if (bDupInfo & FDUP_DATE_MATCH)
        {
            *pch++ = L'D';
            *pch++ = L',';
        }

        if (bDupInfo & FDUP_HASH_MATCH)
        {
            *pch++ = L'H';
            *pch++ = L',';
        }

        // To remove trailing ','
        --pch;

        // Finally, null-terminate
        *pch = 0;
    }
}

int CompareFilesByName(_In_ PVOID pLeftFile, _In_ PVOID pRightFile)
{
    PFILEINFO pLeft = (PFILEINFO)pLeftFile;
    PFILEINFO pRight = (PFILEINFO)pRightFile;

    size_t count = wcsnlen(pLeft->szFilename, ARRAYSIZE(pLeft->szFilename));
    return _wcsnicmp(pLeft->szFilename, pRight->szFilename, count);
}
