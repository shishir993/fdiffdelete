#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "common.h"
#include "DirectoryWalker.h"

HRESULT GetFolderToOpen(_Out_z_cap_(MAX_PATH) PWSTR pszFolderpath);
BOOL GetTextFromEditControl(_In_ HWND hEditControl, _Inout_z_ WCHAR* pszFolderpath, _In_ int nMaxElements);
BOOL GetSelectedLvItemsText(
    _In_ HWND hListView, 
    _Out_ PWSTR *ppaszFiles, 
    _Out_ PINT pnItems, 
    _In_ int nMaxCharsEachItemText);
BOOL BuildDirInfo(_In_z_ PCWSTR pszFolderpath, _Out_ PDIRINFO *ppDirInfo);
BOOL PopulateFileList(_In_ HWND hList, _In_ PDIRINFO pDirInfo);

int CALLBACK lvCmpName(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int CALLBACK lvCmpDupType(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int CALLBACK lvCmpDate(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
int CALLBACK lvCmpSize(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);