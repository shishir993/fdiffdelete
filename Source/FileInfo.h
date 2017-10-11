#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "common.h"
#include "StringFunctions.h"
#include "HashFactory.h"

// **
// File attributes considered for duplicate determination are
// Filename, Filesize, Modified time (and File hash). 
// Bitwise operations with these can help with storing 
// these in the FILEINFO structure.
#define FDUP_NO_MATCH       0x00
#define FDUP_NAME_MATCH     0x01
#define FDUP_SIZE_MATCH     0x02
#define FDUP_DATE_MATCH     0x04
#define FDUP_HASH_MATCH     0x08

// Structure to hold information about a file
typedef struct _FileInfo {
    BOOL fIsDirectory;
    BOOL fAccessDenied;

    // This structure must know about the duplicacy of a file
    // because otherwise the directory must hold an additional
    // list of duplicate files.
    BYTE bDupInfo;

    LARGE_INTEGER llFilesize;
    BYTE abHash[HASHLEN_SHA1];
    SYSTEMTIME stModifiedTime;
    FILETIME ftModifiedTime;

    WCHAR szPath[MAX_PATH];
    WCHAR szFilename[MAX_PATH];

}FILEINFO, *PFILEINFO;

#define ClearDuplicateAttr(pFileInfo)   (pFileInfo->bDupInfo = FDUP_NO_MATCH)

// Functions

HRESULT FileInfoInit(_In_ BOOL fComputeHash);
void FileInfoDestroy();

// Populate file info for the specified file in the caller specified memory location
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _In_ BOOL fComputeHash, _In_ PFILEINFO pFileInfo);

// Populate file info for the specified file in the callee heap-allocated memory location
// and return the pointer to this location to the caller.
BOOL CreateFileInfo(_In_ PCWSTR pszFullpathToFile, _In_ BOOL fComputeHash, _Out_ PFILEINFO* ppFileInfo);

// Compare two file info structs and say whether they are equal or not,
// also set duplicate flag in the file info structs.
BOOL CompareFileInfoAndMark(_In_ const PFILEINFO pLeftFile, _In_ const PFILEINFO pRightFile, _In_ BOOL fCompareHashes);

inline BOOL IsDuplicateFile(_In_ const PFILEINFO pFileInfo);

void GetDupTypeString(_In_ PFILEINFO pFileInfo, _Inout_z_ PWSTR pszDupType);

int CompareFilesByName(_In_ PVOID pLeftFile, _In_ PVOID pRightFile);
