#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "Common.h"
#include "DirectoryWalker_Interface.h"
#include "FileInfo.h"

int StringSizeBytes(_In_ PCWSTR pwsz);
BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars);
BOOL IsDirectoryEmpty(_In_z_ PCWSTR pszPath);

HRESULT DelEmptyFolders_Init(_In_ PDIRINFO pDirDeleteFrom, _Out_ PCHL_HTABLE* pphtFoldersSeen);
void DelEmptyFolders_Add(_In_opt_ PCHL_HTABLE phtFoldersSeen, _In_ PFILEINFO pFile);
void DelEmptyFolders_Delete(_In_opt_ PCHL_HTABLE phtFoldersSeen);
