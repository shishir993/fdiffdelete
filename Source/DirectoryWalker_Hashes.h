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
#include "CHelpLibDll.h"
#include "DirectoryWalker_Interface.h"

// ** Functions **

BOOL BuildDirTree_Hash(_In_z_ PCWSTR pszRootpath, _Out_ PDIRINFO* ppRootDir);

// Build the list of files in the given folder
BOOL BuildFilesInDir_Hash(
    _In_ PCWSTR pszFolderpath, 
    _In_opt_ PCHL_QUEUE pqDirsToTraverse, 
    _Inout_ PDIRINFO* ppDirInfo);

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles_Hash(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir);

// Clear the duplicate flag of all files in the specified directory
void ClearFilesDupFlag_Hash(_In_ PDIRINFO pDirInfo);

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir_Hash(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate);

// Similar to the DeleteDupFilesInDir() function but it deletes only the specified files
// from the pDirDeleteFrom directory and update the other directory files'. The files to
// be deleted are specified in a list of strings that are the file names.
BOOL DeleteFilesInDir_Hash(
    _In_ PDIRINFO pDirDeleteFrom, 
    _In_ PFILEINFO *paFilesToDelete, 
    _In_ int nFiles, 
    _In_ PDIRINFO pDirToUpdate);

// Print the dir tree in BFS order, two blank lines separating 
// file listing of each directory
void PrintDirTree_Hash(_In_ PDIRINFO pRootDir);

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir_Hash(_In_ PDIRINFO pDirInfo);

void DestroyDirInfo_Hash(_In_ PDIRINFO pDirInfo);