
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DirectoryWalker_Interface.h"
#include "DirectoryWalker.h"
#include "DirectoryWalker_Hashes.h"

void DestroyDirInfo(_In_ PDIRINFO pDirInfo)
{
    if (pDirInfo->fHashCompare)
    {
        DestroyDirInfo_Hash(pDirInfo);
    }
    else
    {
        DestroyDirInfo_NoHash(pDirInfo);
    }
}

BOOL BuildDirTree(_In_z_ PCWSTR pszRootpath, _In_ BOOL fCompareHashes, _Out_ PDIRINFO* ppRootDir)
{
    BOOL fRetVal;
    if (fCompareHashes)
    {
        fRetVal = BuildDirTree_Hash(pszRootpath, ppRootDir);
    }
    else
    {
        fRetVal = BuildDirTree_NoHash(pszRootpath, ppRootDir);
    }
    return fRetVal;
}

// Build the list of files in the given folder
BOOL BuildFilesInDir(
    _In_ PCWSTR pszFolderpath,
    _In_opt_ PCHL_QUEUE pqDirsToTraverse,
    _In_ BOOL fCompareHashes,
    _Inout_ PDIRINFO* ppDirInfo)
{
    BOOL fRetVal;
    if (fCompareHashes)
    {
        fRetVal = BuildFilesInDir_Hash(pszFolderpath, pqDirsToTraverse, ppDirInfo);
    }
    else
    {
        fRetVal = BuildFilesInDir_NoHash(pszFolderpath, pqDirsToTraverse, ppDirInfo);
    }
    return fRetVal;
}

// Given two DIRINFO objects, compare the files in them and set each file's
// duplicate flag to indicate that the file is present in both dirs.
BOOL CompareDirsAndMarkFiles(_In_ PDIRINFO pLeftDir, _In_ PDIRINFO pRightDir)
{
    // Both dirs must have been built with or without hash compare
    int numHashEnabled = 0;
    if (pLeftDir->fHashCompare)
        ++numHashEnabled;

    if (pRightDir->fHashCompare)
        ++numHashEnabled;

    if (numHashEnabled == 1)
    {
        logerr(L"Only one of the dirs has hash compare enabled!");
        return FALSE;
    }

    BOOL fRetVal;
    if (numHashEnabled == 2)
    {
        fRetVal = CompareDirsAndMarkFiles_Hash(pLeftDir, pRightDir);
    }
    else
    {
        fRetVal = CompareDirsAndMarkFiles_NoHash(pLeftDir, pRightDir);
    }

    return fRetVal;
}

// Clear the duplicate flag of all files in the specified directory
void ClearFilesDupFlag(_In_ PDIRINFO pDirInfo)
{
    if (pDirInfo->fHashCompare)
    {
        ClearFilesDupFlag_Hash(pDirInfo);
    }
    else
    {
        ClearFilesDupFlag_NoHash(pDirInfo);
    }
}

// Inplace delete of files in a directory. This deletes duplicate files from
// the pDirDeleteFrom directory and removes the duplicate flag of the deleted
// files in the pDirToUpdate directory.
BOOL DeleteDupFilesInDir(_In_ PDIRINFO pDirDeleteFrom, _In_ PDIRINFO pDirToUpdate)
{
    // Both dirs must have been built with or without hash compare
    int numHashEnabled = 0;
    if (pDirDeleteFrom->fHashCompare)
        ++numHashEnabled;

    if (pDirToUpdate->fHashCompare)
        ++numHashEnabled;

    if (numHashEnabled == 1)
    {
        logerr(L"Only one of the dirs has hash compare enabled!");
        return FALSE;
    }

    BOOL fRetVal;
    if (numHashEnabled == 2)
    {
        fRetVal = DeleteDupFilesInDir_Hash(pDirDeleteFrom, pDirToUpdate);
    }
    else
    {
        fRetVal = DeleteDupFilesInDir_NoHash(pDirDeleteFrom, pDirToUpdate);
    }

    return fRetVal;
}

// Similar to the DeleteDupFilesInDir() function but it deletes only the specified files
// from the pDirDeleteFrom directory and update the other directory files'. The files to
// be deleted are specified in a list of strings that are the file names.
BOOL DeleteFilesInDir(
    _In_ PDIRINFO pDirDeleteFrom,
    _In_ PCWSTR paszFileNamesToDelete,
    _In_ int nFileNames,
    _In_ PDIRINFO pDirToUpdate)
{
    SB_ASSERT(!pDirDeleteFrom->fHashCompare && (pDirToUpdate == NULL || !pDirToUpdate->fHashCompare));
    return DeleteFilesInDir_NoHash(pDirDeleteFrom, paszFileNamesToDelete, nFileNames, pDirToUpdate);
}

BOOL DeleteFilesInDir(
    _In_ PDIRINFO pDirDeleteFrom,
    _In_ PFILEINFO *paFilesToDelete,
    _In_ int nFiles,
    _In_ PDIRINFO pDirToUpdate)
{
    SB_ASSERT(pDirDeleteFrom->fHashCompare && (pDirToUpdate == NULL || pDirToUpdate->fHashCompare));
    return DeleteFilesInDir_Hash(pDirDeleteFrom, paFilesToDelete, nFiles, pDirToUpdate);
}

// Print the dir tree in BFS order, two blank lines separating 
// file listing of each directory
void PrintDirTree(_In_ PDIRINFO pRootDir)
{
    if (pRootDir->fHashCompare)
    {
        PrintDirTree_Hash(pRootDir);
    }
    else
    {
        PrintDirTree_NoHash(pRootDir);
    }
}

// Print files in the folder, one on each line. End on a blank line.
void PrintFilesInDir(_In_ PDIRINFO pDirInfo)
{
    if (pDirInfo->fHashCompare)
    {
        PrintFilesInDir_Hash(pDirInfo);
    }
    else
    {
        PrintFilesInDir_NoHash(pDirInfo);
    }
}
