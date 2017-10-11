
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
