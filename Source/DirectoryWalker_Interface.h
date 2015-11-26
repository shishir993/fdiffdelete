#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "Common.h"
#include "FileInfo.h"

// Structure to hold the files that have same name within
// the same directory tree. This is required because the hashtable
// in DIRINFO uses filename as key, which means there will be name
// conflicts.
typedef struct _dupWithin
{
    int nCurFiles;
    int nCurSize;
    PFILEINFO *apFiles;
}DUPFILES_WITHIN, *PDUPFILES_WITHIN;

typedef struct _DirectoryInfo
{
    WCHAR pszPath[MAX_PATH];
    int nDirs;
    int nFiles;

    // Was hash comapre used when this DIRINFO was built?
    BOOL fHashCompare;

    // Use a hashtable to store file list.
    // Key is the filename, value is a FILEINFO structure - if hash compare is turned OFF
    // Key is hash string, value is a PCHL_LLIST that is the
    // linked list of files with the same hash string - if hash compare is turned ON
    CHL_HTABLE *phtFiles;

    // List of FILEINFO of files that have the same name in 
    // the same dir tree. - if hash compare is turned OFF
    DUPFILES_WITHIN stDupFilesInTree;

}DIRINFO, *PDIRINFO;

// ** Functions **

BOOL BuildDirTree(_In_z_ PCWSTR pszRootpath, _In_ BOOL fCompareHashes, _Out_ PDIRINFO* ppRootDir);

// Build the list of files in the given folder
BOOL BuildFilesInDir(
    _In_ PCWSTR pszFolderpath, 
    _In_opt_ PCHL_QUEUE pqDirsToTraverse, 
    _In_ BOOL fCompareHashes,
    _Inout_ PDIRINFO* ppDirInfo);

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

BOOL DeleteFilesInDir(
    _In_ PDIRINFO pDirDeleteFrom, 
    _In_ PFILEINFO *paFilesToDelete, 
    _In_ int nFiles, 
    _In_ PDIRINFO pDirToUpdate);

// Print the dir tree in BFS order, two blank lines separating 
// file listing of each directory
void PrintDirTree(_In_ PDIRINFO pRootDir);

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir(_In_ PDIRINFO pDirInfo);

void DestroyDirInfo(_In_ PDIRINFO pDirInfo);
