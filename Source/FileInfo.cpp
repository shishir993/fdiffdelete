
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "FileInfo.h"

// Populate file info for the specified file in the caller specified memory location
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _In_ PFILEINFO pFileInfo)
{
    //NT_ASSERT(pszFullpathToFile);
    //NT_ASSERT(pFileInfo);

    ZeroMemory(pFileInfo, sizeof(*pFileInfo));

    // Copy file/dir name over to the FILEINFO struct
    // TODO: Temporary work around of casting since the callee doesn't take a PCWSTR
    WCHAR* pszFilename = pszChlSzGetFilenameFromPath((WCHAR*)(void*)pszFullpathToFile, wcslen(pszFullpathToFile));
    wcscpy_s(pFileInfo->szFilename, ARRAYSIZE(pFileInfo->szFilename), pszFilename);

    // Check if file is a directory
    // TODO: To extend this limit to 32,767 wide characters, 
    // call the Unicode version of the function and prepend "\\?\" to the path
    DWORD dwAttr = GetFileAttributes(pszFullpathToFile);
    if(dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        logerr(L"Unable to get attributes of file: %s", pszFullpathToFile);
        goto error_return;
    }

    if(dwAttr & FILE_ATTRIBUTE_DIRECTORY)
    {
        pFileInfo->fIsDirectory = TRUE;
    }

    // FILE_FLAG_BACKUP_SEMANTICS specified to get handle to directory also
    HANDLE hFile = CreateFileW(pszFullpathToFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
    {
        if(GetLastError() == ERROR_ACCESS_DENIED)
        {
            logwarn(L"Read access denied to file: %s", pszFullpathToFile);
            goto done;
        }
        logerr(L"Failed to open file %s", pszFullpathToFile);
        goto error_return;
    }

    if(!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        // Query file size
        if(!GetFileSizeEx(hFile, &pFileInfo->llFilesize))
        {
            logerr(L"Failed to get size of %s", pszFullpathToFile);
            goto error_return;
        }    
    }

    SYSTEMTIME stUTC;
    if(!GetFileTime(hFile, NULL, NULL, &pFileInfo->ftModifiedTime))
    {
        logerr(L"Failed to get modified datetime of %s", pszFullpathToFile);
        goto error_return;
    }

    FileTimeToSystemTime(&pFileInfo->ftModifiedTime, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &pFileInfo->stModifiedTime);

    CloseHandle(hFile);

done:
    return TRUE;

error_return:
    if(hFile != NULL && hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);
        hFile = NULL;
    }

    return FALSE;
}

// Populate file info for the specified file in the callee heap-allocated memory location
// and return the pointer to this location to the caller.
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _Out_ PFILEINFO* ppFileInfo)
{
    //NT_ASSERT(pszFullpathToFile);
    //NT_ASSERT(ppFileInfo);

    PFILEINFO pFileInfo;
    pFileInfo = (PFILEINFO)malloc(sizeof(FILEINFO));
    if(pFileInfo == NULL)
    {
        goto error_return;
    }

    if(!CreateFileInfo(pszFullpathToFile, pFileInfo))
    {
        goto error_return;
    }
    
    *ppFileInfo = pFileInfo;
    return TRUE;

error_return:
    if(pFileInfo != NULL)
    {
        free(pFileInfo);
    }

    *ppFileInfo = NULL;
    return FALSE;
}

// Compare two file info structs and say whether they are equal or not
// also set duplicate flag in the file info structs.
BOOL CompareFileInfoAndMark(_In_ const PFILEINFO pLeftFile, _In_ const PFILEINFO pRightFile)
{
    //NT_ASSERT(pLeftFile);
    //NT_ASSERT(pRightFile);

    // Along with setting bits, must also clear bits 
    // in case of no match because:
    // User could have chosen two folders first and then
    // changed one of the folders which would invaliate
    // the unchanged folder's files' duplicacy.

    // Until recursive directory compare is implemented, two directories match
    // only if their names are the same.
    if(pLeftFile->fIsDirectory && pRightFile->fIsDirectory)
    {
        if(_wcsnicmp(pLeftFile->szFilename, pRightFile->szFilename, MAX_PATH) == 0)
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
    else if(pLeftFile->fIsDirectory || pRightFile->fIsDirectory)
    {
        // Only one of them is a directory, there can no match except, but a
        // name match between a file and directory isn't very useful
        pLeftFile->bDupInfo = FDUP_NO_MATCH;
        pRightFile->bDupInfo = FDUP_NO_MATCH;
    }
    else
    {
        // Both are files, perform the various match checks

        if(_wcsnicmp(pLeftFile->szFilename, pRightFile->szFilename, MAX_PATH) == 0)
        {
            pLeftFile->bDupInfo |= FDUP_NAME_MATCH;
            pRightFile->bDupInfo |= FDUP_NAME_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_NAME_MATCH);
            pRightFile->bDupInfo &= (~FDUP_NAME_MATCH);
        }

        if(pLeftFile->llFilesize.QuadPart == pRightFile->llFilesize.QuadPart)
        {
            pLeftFile->bDupInfo |= FDUP_SIZE_MATCH;
            pRightFile->bDupInfo |= FDUP_SIZE_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_SIZE_MATCH);
            pRightFile->bDupInfo &= (~FDUP_SIZE_MATCH);
        }

        if(memcmp(&pLeftFile->stModifiedTime, &pRightFile->stModifiedTime, sizeof(pLeftFile->stModifiedTime)) == 0)
        {
            pLeftFile->bDupInfo |= FDUP_DATE_MATCH;
            pRightFile->bDupInfo |= FDUP_DATE_MATCH;
        }
        else
        {
            pLeftFile->bDupInfo &= (~FDUP_DATE_MATCH);
            pRightFile->bDupInfo &= (~FDUP_DATE_MATCH);
        }
    }

    return IsDuplicateFile(pLeftFile);
}

inline BOOL IsDuplicateFile(_In_ const PFILEINFO pFileInfo)
{
    // File is a duplicate if:
    // 1. Hash matches OR
    // 2. Name and size and date modified matches
    BYTE bDupInfo = pFileInfo->bDupInfo;
    return (bDupInfo & FDUP_NAME_MATCH) && 
           (bDupInfo & FDUP_SIZE_MATCH) &&
           (bDupInfo & FDUP_DATE_MATCH);
}

void GetDupTypeString(_In_ PFILEINFO pFileInfo, _Inout_z_ PWSTR pszDupType)
{
    PWCHAR pch = pszDupType;

    *pch = 0;

    BYTE bDupInfo = pFileInfo->bDupInfo;
    if(bDupInfo != FDUP_NO_MATCH)
    {
        if(bDupInfo & FDUP_NAME_MATCH)
        {
            *pch++ = L'N';
            *pch++ = L',';
        }

        if(bDupInfo & FDUP_SIZE_MATCH)
        {
            *pch++ = L'S';
            *pch++ = L',';
        }

        if(bDupInfo & FDUP_DATE_MATCH)
        {
            *pch++ = L'D';
            *pch++ = L',';
        }

        if(bDupInfo & FDUP_HASH_MATCH)
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
