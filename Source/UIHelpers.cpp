
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "UIHelpers.h"
#include <Shobjidl.h>
#include <ShellAPI.h>
#include "FileInfo.h"

#define ONE_KBYTES    1024ll
#define ONE_MBYTES    (ONE_KBYTES * 1024ll)
#define ONE_GBYTES    (ONE_MBYTES * 1024ll)

static BOOL PopulateFileList(_In_ HWND hList, _In_ PDIRINFO pDirInfo);
static void ConstructListViewRow(_In_ PFILEINFO pFileInfo, _In_ PWSTR *apsz);

HRESULT GetFolderToOpen(_Out_z_cap_(MAX_PATH) PWSTR pszFolderpath)
{
    // Sample from: http://msdn.microsoft.com/en-us/library/windows/desktop/ff485843%28v=vs.85%29.aspx

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(SUCCEEDED(hr))
    {
        // Create the FileOpenDialog object.
        IFileOpenDialog *pFileOpen;
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, 
                IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        if(SUCCEEDED(hr))
        {
            // Set to pick a folder (not a file)
            FILEOPENDIALOGOPTIONS fos;
            hr = pFileOpen->GetOptions(&fos);
            if(SUCCEEDED(hr))
            {
                fos |= FOS_PICKFOLDERS;
                hr = pFileOpen->SetOptions(fos);
            }

            // Show the Open dialog box.
            if(SUCCEEDED(hr))
            {
                hr = pFileOpen->Show(NULL);
            }

            // Get the folder selected from the dialog box.
            if(SUCCEEDED(hr))
            {
                IShellItem *pItem;
                hr = pFileOpen->GetResult(&pItem);
                if(SUCCEEDED(hr))
                {
                    PWSTR pszPathChosen;
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPathChosen);
                    if(SUCCEEDED(hr))
                    {
                        if(wcslen(pszPathChosen) >= MAX_PATH)
                        {
                            MessageBox(NULL, L"Path too long. Exceeds 259 characters.", L"File Path", MB_OK);
                            hr = E_NOT_SUFFICIENT_BUFFER;
                        }
                        else
                        {
                            wcscpy_s(pszFolderpath, MAX_PATH, pszPathChosen);
                        }

                        CoTaskMemFree(pszPathChosen);
                    }
                    pItem->Release();
                }
            }
            pFileOpen->Release();
        }
        CoUninitialize();
    }

    return hr;
}

BOOL GetTextFromEditControl(_In_ HWND hEditControl, _Inout_z_ WCHAR* pszFolderpath, _In_ int nMaxElements)
{
    // Unselect the edit control first
    SendMessage(hEditControl, EM_SETSEL, -1, 0);

    // Find number of characters present in the edit control
    int nChars = SendMessage(hEditControl, EM_LINELENGTH, -1, (LPARAM)NULL);
    if(nChars == 0)
    {
        pszFolderpath[0] = 0;
        return TRUE;
    }

    if(nChars >= nMaxElements)
    {
        MessageBox(NULL, L"Path too long.", L"Error", MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }

    // Finally, get the text
    nChars = SendMessage(hEditControl, WM_GETTEXT, nMaxElements, (LPARAM)pszFolderpath);
    pszFolderpath[nChars] = 0;
    return TRUE;
}

BOOL IsMenuItemChecked(_In_ HMENU hMenu, _In_ UINT uiItemId)
{
    BOOL fRetVal = FALSE;

    UINT uiState = GetMenuState(hMenu, uiItemId, MF_BYCOMMAND);
    if(uiState != -1)
    {
        fRetVal = (uiState & MF_CHECKED) ? TRUE : FALSE;
    }
    else
    {
        logerr(L"GetMenuState() failed.");
    }
    return fRetVal;
}

BOOL GetSelectedLvItemsText(
    _In_ HWND hListView, 
    _Out_ PWSTR *ppaszFiles,
    _Out_ PINT pnItems, 
    _In_ int nMaxCharsEachItemText)
{
    // Get all selected items from the list view
    UINT nItems = SendMessage(hListView, LVM_GETSELECTEDCOUNT, 0, (LPARAM)NULL);
    if(nItems <= 0)
    {
        goto error_return;
    }

    // TODO: Check for overflow in the below calculation
    PWSTR pasz = NULL;
    pasz = (PWSTR)malloc(nItems * nMaxCharsEachItemText * sizeof(WCHAR));
    if(pasz == NULL)
    {
        logerr(L"Out of memory.");
        goto error_return;
    }

    int iSelected = -1;
    int iStringIdx = 0;
    while((iSelected = SendMessage(hListView, LVM_GETNEXTITEM, iSelected, LVIS_SELECTED)) != -1)
    {
        LVITEM lvItem = {0};

        lvItem.cchTextMax = MAX_PATH;
        lvItem.pszText = pasz + (iStringIdx * nMaxCharsEachItemText);
        SendMessage(hListView, LVM_GETITEMTEXT, iSelected, (LPARAM)&lvItem);
        logdbg(L"Selected item: %s", pasz + (iStringIdx * nMaxCharsEachItemText));

        ++iStringIdx;
    }

    *ppaszFiles = pasz;
    *pnItems = nItems;
    return TRUE;

error_return:
    *pnItems = 0;
    *ppaszFiles = NULL;
    return FALSE;
}

BOOL GetSelectedLvItemsText_Hash(
    _In_ HWND hListView, 
    _Out_ PFILEINFO **pppaFileInfo,
    _Out_ PINT pnItems)
{
    // Get all selected items from the list view
    UINT nItems = SendMessage(hListView, LVM_GETSELECTEDCOUNT, 0, (LPARAM)NULL);
    if(nItems <= 0)
    {
        goto error_return;
    }

    // TODO: Check for overflow in the below calculation
    PFILEINFO *ppaFiles = NULL;
    ppaFiles = (PFILEINFO*)malloc(nItems * sizeof PFILEINFO);
    if(ppaFiles == NULL)
    {
        logerr(L"Out of memory.");
        goto error_return;
    }

    int iSelected = -1;
    int iIdx = 0;
    while((iSelected = SendMessage(hListView, LVM_GETNEXTITEM, iSelected, LVIS_SELECTED)) != -1)
    {
        LVITEM lvItem = {0};
        lvItem.iItem = iSelected;
        lvItem.mask = LVIF_PARAM;
        SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lvItem);

        PFILEINFO pFileSel = (PFILEINFO)lvItem.lParam;
        logdbg(L"Selected item: %s", pFileSel->szFilename);

        ppaFiles[iIdx++] = pFileSel;
    }

    *pppaFileInfo = ppaFiles;
    *pnItems = nItems;
    return TRUE;

error_return:
    *pnItems = 0;
    *pppaFileInfo = NULL;
    return FALSE;
}

BOOL BuildDirInfo(
    _In_z_ PCWSTR pszFolderpath,
    _In_ BOOL fRecursive,
    _In_ BOOL fCompareHashes,
    _Out_ PDIRINFO *ppDirInfo)
{
    // Check if folder exists or not
    if(!PathFileExists(pszFolderpath))
    {
        logerr(L"Folder not found: %s", pszFolderpath);
        return FALSE;
    }
    
    if(fCompareHashes)
    {
        if(FAILED(FileInfoInit(TRUE)))
        {
            logerr(L"Cannot initialize FileInfo for hash comparisons.");
            return FALSE;
        }
    }

    PDIRINFO pDir = NULL;
    if(fRecursive)
    {
        if(!BuildDirTree(pszFolderpath, fCompareHashes, &pDir))
        {
            logerr(L"Cannot recursive build files in folder: %s", pszFolderpath);
            goto error_return;
        }
    }
    else
    {
        if(!BuildFilesInDir(pszFolderpath, NULL, fCompareHashes, &pDir))
        {
            logerr(L"Cannot list files in folder: %s", pszFolderpath);
            goto error_return;
        }
    }

    *ppDirInfo = pDir;
    return TRUE;

error_return:
    if(pDir)
    {
        DestroyDirInfo(pDir);
    }

    *ppDirInfo = NULL;
    return FALSE;
}

static BOOL PopulateFileList(_In_ HWND hList, _In_ PDIRINFO pDirInfo)
{
    ListView_DeleteAllItems(hList);
    if(pDirInfo->nFiles <= 0)
    {
        return TRUE;
    }

    CHL_HT_ITERATOR itr;
    CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr);

    char* pszKey;
    PFILEINFO pFileInfo;
    int nKeySize, nValSize;

    WCHAR szDupType[10];    // N,S,D,H (name, size, date, hash)
    WCHAR szDateTime[32];   // 08/13/2014 5:55 PM
    WCHAR szSize[16];       // formatted to show KB, MB, GB
    
    PWCHAR apszListRow[] = {NULL, szDupType, NULL, szDateTime, szSize};

    // For each file in the dir, convert relevant attributes into 
    // strings and insert into the list view row by row.
    BOOL fRetVal = TRUE;
    while(SUCCEEDED(CHL_DsGetNextHT(&itr, &pszKey, &nKeySize, &pFileInfo, &nValSize, TRUE)))
    {
        ConstructListViewRow(pFileInfo, apszListRow);
        if(FAILED(CHL_GuiAddListViewRow(hList, apszListRow, ARRAYSIZE(apszListRow), (LPARAM)pFileInfo)))
        {
            logerr(L"Error inserting into file list");
            fRetVal = FALSE;
            break;
        }
    }

    // Insert the name-duplicate files now.
    for(int i = 0; i < pDirInfo->stDupFilesInTree.nCurFiles; ++i)
    {
		if (FAILED(CHL_DsReadRA(&pDirInfo->stDupFilesInTree.aFiles, i, &pFileInfo, NULL, TRUE)))
		{
			continue;
		}

        ConstructListViewRow(pFileInfo, apszListRow);
        if(FAILED(CHL_GuiAddListViewRow(hList, apszListRow, ARRAYSIZE(apszListRow), (LPARAM)pFileInfo)))
        {
            logerr(L"Error inserting into file list");
            fRetVal = FALSE;
            break;
        }
    }

    // Clear list view if there was an error.
    if(!fRetVal)
    {
        ListView_DeleteAllItems(hList);
    }

    return fRetVal;
}

BOOL PopulateFileList(_In_ HWND hList, _In_ PDIRINFO pDirInfo, _In_ BOOL fCompareHashes)
{
    if(!fCompareHashes)
    {
        return PopulateFileList(hList, pDirInfo);
    }

    ListView_DeleteAllItems(hList);
    if(pDirInfo->nFiles <= 0)
    {
        return TRUE;
    }

    CHL_HT_ITERATOR itr;
    CHL_DsInitIteratorHT(pDirInfo->phtFiles, &itr);

    WCHAR szDupType[10];    // N,S,D,H (name, size, date, hash)
    WCHAR szDateTime[32];   // 08/13/2014 5:55 PM
    WCHAR szSize[16];       // formatted to show KB, MB, GB
    
    PWCHAR apszListRow[] = {NULL, szDupType, NULL, szDateTime, szSize};

    // For each file in the dir, convert relevant attributes into 
    // strings and insert into the list view row by row.
    BOOL fRetVal = TRUE;

    char* pszKey;
    PCHL_LLIST pList = NULL;
    while(SUCCEEDED(CHL_DsGetNextHT(&itr, &pszKey, NULL, &pList, NULL, TRUE)))
    {
        // Foreach file in the linked list, insert into list view
        for(int i = 0; i < pList->nCurNodes; ++i)
        {
			PFILEINFO pFileInfo = NULL;
			int ptrSize = sizeof(pFileInfo);
            if(FAILED(CHL_DsPeekAtLL(pList, i, &pFileInfo, &ptrSize, FALSE)))
            {
                logerr(L"Cannot get item %d for hash string %S", i, pszKey);
                continue;
            }

            ConstructListViewRow(pFileInfo, apszListRow);
            if(FAILED(CHL_GuiAddListViewRow(hList, apszListRow, ARRAYSIZE(apszListRow), (LPARAM)pFileInfo)))
            {
                logerr(L"Error inserting into file list");
                fRetVal = FALSE;
                break;
            }
        }
    }

    // Clear list view if there was an error.
    if(!fRetVal)
    {
        ListView_DeleteAllItems(hList);
    }

    return fRetVal;
}

static void ConstructListViewRow(_In_ PFILEINFO pFileInfo, _In_ PWSTR *apsz)
{
    apsz[0] = pFileInfo->szFilename;
    GetDupTypeString(pFileInfo, apsz[1]);
    apsz[2] = pFileInfo->szPath;

    swprintf_s(apsz[3], 32, L"%02d/%02d/%d  %02d:%02d",
        pFileInfo->stModifiedTime.wMonth, pFileInfo->stModifiedTime.wDay, pFileInfo->stModifiedTime.wYear,
        pFileInfo->stModifiedTime.wHour, pFileInfo->stModifiedTime.wMinute);

    LONGLONG llFileSize = pFileInfo->llFilesize.QuadPart;

    PWCHAR pszSizeMarker;
    if(pFileInfo->fIsDirectory)
    {
        *(apsz[4]) = 0;
    }
    else
    {
        static WCHAR aszSizeMarkers[][3] = {L"B", L"KB", L"MB", L"GB"};
        if(llFileSize >= ONE_GBYTES)
        {
            llFileSize /= ONE_GBYTES;
            pszSizeMarker = aszSizeMarkers[3];
        }
        else if(llFileSize >= ONE_MBYTES)
        {
            llFileSize /= ONE_MBYTES;
            pszSizeMarker = aszSizeMarkers[2];
        }
        else if(llFileSize >= ONE_GBYTES)
        {
            llFileSize /= ONE_KBYTES;
            pszSizeMarker = aszSizeMarkers[1];
        }
        else
        {
            pszSizeMarker = aszSizeMarkers[0];
        }

        swprintf_s(apsz[4], 16, L"%lld %s", llFileSize, pszSizeMarker);
    }
}

#pragma region LvCompares

// ** Listview sorting Callbacks
// lParam1: is the value associated with the first item being compared
// lParam2: is the value associated with the first second being compared
// lParamSort: is the same value passed to the LVM_SORTITEMS message
// The comparison function must return a negative value if the first item should precede the second, 
// a positive value if the first item should follow the second, or zero if the two items are equivalent. 
// Note: During the sorting process, the list-view contents are unstable. If the callback function sends any messages to the list-view control, the results are unpredictable. 

int CALLBACK lvCmpName(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    LVITEM lv1;
    LVITEM lv2;

    ZeroMemory(&lv1, sizeof(lv1));
    ZeroMemory(&lv2, sizeof(lv2));

    lv1.iItem = (int)lParam1;
    lv1.mask = LVIF_PARAM;

    lv2.iItem = (int)lParam2;
    lv2.mask = LVIF_PARAM;

    HWND hList = (HWND)lParamSort;
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv1);
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv2);

    return _wcsnicmp(((PFILEINFO)lv1.lParam)->szFilename, ((PFILEINFO)lv2.lParam)->szFilename, MAX_PATH);
}

int CALLBACK lvCmpDupType(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    LVITEM lv1;
    LVITEM lv2;

    ZeroMemory(&lv1, sizeof(lv1));
    ZeroMemory(&lv2, sizeof(lv2));

    lv1.iItem = (int)lParam1;
    lv1.mask = LVIF_PARAM;

    lv2.iItem = (int)lParam2;
    lv2.mask = LVIF_PARAM;

    HWND hList = (HWND)lParamSort;
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv1);
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv2);
    
    PFILEINFO pf1 = (PFILEINFO)lv1.lParam;
    PFILEINFO pf2 = (PFILEINFO)lv2.lParam;
    if(pf1->bDupInfo == pf2->bDupInfo)
        return 0;

    // Duplicate file must be higher than non-dup
    if(IsDuplicateFile(pf1))
        return -1;
        
    return 1;
}

int CALLBACK lvCmpPath(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    LVITEM lv1;
    LVITEM lv2;

    ZeroMemory(&lv1, sizeof(lv1));
    ZeroMemory(&lv2, sizeof(lv2));

    lv1.iItem = (int)lParam1;
    lv1.mask = LVIF_PARAM;

    lv2.iItem = (int)lParam2;
    lv2.mask = LVIF_PARAM;

    HWND hList = (HWND)lParamSort;
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv1);
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv2);

    return _wcsnicmp(((PFILEINFO)lv1.lParam)->szPath, ((PFILEINFO)lv2.lParam)->szPath, MAX_PATH);
}

int CALLBACK lvCmpDate(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    LVITEM lv1;
    LVITEM lv2;

    ZeroMemory(&lv1, sizeof(lv1));
    ZeroMemory(&lv2, sizeof(lv2));

    lv1.iItem = (int)lParam1;
    lv1.mask = LVIF_PARAM;

    lv2.iItem = (int)lParam2;
    lv2.mask = LVIF_PARAM;

    HWND hList = (HWND)lParamSort;
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv1);
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv2);
    
    PFILEINFO pf1 = (PFILEINFO)lv1.lParam;
    PFILEINFO pf2 = (PFILEINFO)lv2.lParam;

    LARGE_INTEGER ll1;
    LARGE_INTEGER ll2;
    ll1.HighPart = pf1->ftModifiedTime.dwHighDateTime;
    ll1.LowPart = pf1->ftModifiedTime.dwLowDateTime;

    ll2.HighPart = pf2->ftModifiedTime.dwHighDateTime;
    ll2.LowPart = pf2->ftModifiedTime.dwLowDateTime;

    if(ll1.QuadPart == ll2.QuadPart)
    {
        return 0;
    }

    if(ll1.QuadPart < ll2.QuadPart)
    {
        return -1;
    }

    return 1;
}

int CALLBACK lvCmpSize(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    LVITEM lv1;
    LVITEM lv2;

    ZeroMemory(&lv1, sizeof(lv1));
    ZeroMemory(&lv2, sizeof(lv2));

    lv1.iItem = (int)lParam1;
    lv1.mask = LVIF_PARAM;

    lv2.iItem = (int)lParam2;
    lv2.mask = LVIF_PARAM;

    HWND hList = (HWND)lParamSort;
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv1);
    SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lv2);
    
    PFILEINFO pf1 = (PFILEINFO)lv1.lParam;
    PFILEINFO pf2 = (PFILEINFO)lv2.lParam;

    if(pf1->llFilesize.QuadPart == pf2->llFilesize.QuadPart)
    {
        return 0;
    }

    if(pf1->llFilesize.QuadPart < pf2->llFilesize.QuadPart)
    {
        return -1;
    }

    return 1;
}

#pragma endregion LvCompares
