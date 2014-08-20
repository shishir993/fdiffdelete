#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "common.h"
#include "FileInfo.h"
#include "CHelpLibDll.h"

typedef struct _DirectoryInfo
{
    WCHAR pszPath[MAX_PATH];
    int nDirs;
    int nFiles;

    // Use a hashtable to store file list.
    // Key is the filename, value is a FILEINFO structure
    CHL_HTABLE *phtFiles;

    struct _dupWithin
    {
        int nCurFiles;
        int nCurSize;
        PFILEINFO *apFiles;
    }stDupWithin;

}DIRINFO, *PDIRINFO;


// ** Functions **
BOOL BuildDirTree(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir);

// Build the list of files in the given folder
BOOL BuildFilesInDir(_In_ PCWSTR pszFolderpath, _In_opt_ PCHL_QUEUE pqDirsToTraverse, _Inout_ PDIRINFO* ppDirInfo);

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir);

// Clear the duplicate flag of all files in the specified directory
void ClearFilesDupFlag(_In_ PDIRINFO pDirInfo);

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate);

// Similar to the DeleteDupFilesInDir() function but it deletes only the specified files
// from the pDirDeleteFrom directory and update the other directory files'. The files to
// be deleted are specified in a list of strings that are the file names.
BOOL DeleteFilesInDir(
    _In_ PDIRINFO pDirDeleteFrom, 
    _In_ PCWSTR paszFileNamesToDelete, 
    _In_ int nFileNames, 
    _In_ PDIRINFO pDirToUpdate);

// Print the dir tree in BFS order, two blank lines separating 
// file listing of each directory
void PrintDirTree(_In_ PDIRINFO pRootDir);

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir(_In_ PDIRINFO pDirInfo);

void DestroyDirTree(_In_ PDIRINFO pRootDir);
void DestroyDirInfo(_In_ PDIRINFO pDirInfo);
