
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
    for(int i = 0; i < ARRAYSIZE(g_apszBannedFilesFolders); ++i)
    {
        if(_wcsnicmp(pszFilename, g_apszBannedFilesFolders[i], nMaxChars) == 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}
