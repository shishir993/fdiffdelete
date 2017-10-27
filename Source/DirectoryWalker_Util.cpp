
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker_Util.h"

static WCHAR *g_apszBannedFilesFolders[] =
{
    L".",
    L"..",
    L"$RECYCLE.BIN",
    L"System Volume Information",

    L"desktop.ini"
};

int StringSizeBytes(_In_ PCWSTR pwsz)
{
    size_t cch = wcslen(pwsz) + 1;
    return (int)(cch * sizeof(WCHAR));
}

BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars)
{
    for (int i = 0; i < ARRAYSIZE(g_apszBannedFilesFolders); ++i)
    {
        if (_wcsnicmp(pszFilename, g_apszBannedFilesFolders[i], nMaxChars) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsDirectoryEmpty(_In_z_ PCWSTR pszPath)
{
    // In order to list all files within the specified directory,
    // path sent to FindFirstFile must end with a "\\*"
    WCHAR szSearchpath[MAX_PATH] = L"";
    wcscpy_s(szSearchpath, ARRAYSIZE(szSearchpath), pszPath);

    int nLen = wcsnlen(pszPath, MAX_PATH);
    if (nLen > 2 && wcsncmp(pszPath + nLen - 2, L"\\*", MAX_PATH) != 0)
    {
        PathCchCombine(szSearchpath, ARRAYSIZE(szSearchpath), pszPath, L"*");
    }

    // Initialize search for files in folder
    WIN32_FIND_DATA findData;
    HANDLE hFindFile = FindFirstFile(szSearchpath, &findData);
    if ((hFindFile == INVALID_HANDLE_VALUE) && (GetLastError() == ERROR_FILE_NOT_FOUND))
    {
        // No files found under the folder. Just return.
        return TRUE;
    }

    if (hFindFile == INVALID_HANDLE_VALUE)
    {
        logerr(L"FindFirstFile failed, err: %u", GetLastError());
        return FALSE;
    }

    do
    {
        if (!((wcscmp(findData.cFileName, L".") == 0)
            || (wcscmp(findData.cFileName, L"..") == 0)))
        {
            FindClose(hFindFile);
            return FALSE;
        }
    } while (FindNextFile(hFindFile, &findData));

    BOOL fEmpty = (GetLastError() == ERROR_NO_MORE_FILES) ? TRUE : FALSE;
    FindClose(hFindFile);
    return fEmpty;
}

HRESULT DelEmptyFolders_Init(_In_ PDIRINFO pDirDeleteFrom, _Out_ PCHL_HTABLE* pphtFoldersSeen)
{
    HRESULT hr = S_OK;
    *pphtFoldersSeen = NULL;
    if (pDirDeleteFrom->fDeleteEmptyDirs == TRUE)
    {
        int nEstEntries = min(1, pDirDeleteFrom->nDirs);
        hr = CHL_DsCreateHT(pphtFoldersSeen, nEstEntries, CHL_KT_WSTRING, CHL_VT_INT32, FALSE);
    }
    return hr;
}

void DelEmptyFolders_Add(_In_opt_ PCHL_HTABLE phtFoldersSeen, _In_ PFILEINFO pFile)
{
    if (phtFoldersSeen == NULL)
    {
        return;
    }

    WCHAR szDir[MAX_PATH];
    HRESULT hr;
    if (pFile->fIsDirectory == TRUE)
    {
        hr = PathCchCombine(szDir, ARRAYSIZE(szDir), pFile->szPath, pFile->szFilename);
    }
    else
    {
        hr = StringCchCopy(szDir, ARRAYSIZE(szDir), pFile->szPath);
    }

    if (SUCCEEDED(hr))
    {
        hr = phtFoldersSeen->Insert(phtFoldersSeen, (PCVOID)szDir, 0, 0, 0);
    }

    if (FAILED(hr))
    {
        logwarn(L"Failed to record as a seen dir, hr: %x, %s\\%s", hr, pFile->szPath, pFile->szFilename);
    }
}

void DelEmptyFolders_Delete(_In_opt_ PCHL_HTABLE phtFoldersSeen)
{
    if (phtFoldersSeen == NULL)
    {
        return;
    }

    /*
     This is just a hashtable of folders and not a tree. So we cannot use DFS logic of checking for
     empty folders. So we need to iterate through the hashtable in search of empty folders until
     an iteration where we do not find any empty folders.
    */

    int nFoldersRemoved;
    do
    {
        nFoldersRemoved = 0;

        CHL_HT_ITERATOR itr;
        HRESULT hr = phtFoldersSeen->InitIterator(phtFoldersSeen, &itr);
        if (FAILED(hr))
        {
            break;
        }

        PCWSTR pszDir = NULL;
        PCWSTR pszPrevDir = NULL;

        do
        {
            itr.GetCurrent(&itr, &pszDir, NULL, NULL, NULL, TRUE);

            // The hashtable iterator expects the currently found entry
            // not to be removed until we move to the next entry. Hence,
            // this logic with previous pointer.
            if (pszPrevDir != NULL)
            {
                phtFoldersSeen->Remove(phtFoldersSeen, (PCVOID)pszPrevDir, 0);
                pszPrevDir = NULL;
            }

            if (IsDirectoryEmpty(pszDir) == TRUE)
            {
                pszPrevDir = pszDir;
                hr = RemoveDirectory(pszDir) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
                loginfo(L"Attempt to remove empty dir, hr: %x, %s", hr, pszDir);
                ++nFoldersRemoved;
            }

        } while (SUCCEEDED(itr.MoveNext(&itr)));

        if (pszPrevDir != NULL)
        {
            phtFoldersSeen->Remove(phtFoldersSeen, (PCVOID)pszPrevDir, 0);
        }
    } while (0 < nFoldersRemoved);

    phtFoldersSeen->Destroy(phtFoldersSeen);
}
