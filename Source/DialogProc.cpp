
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DialogProc.h"
#include "resource.h"
#include "CHelpLibDll.h"
#include "UIHelpers.h"

#define WM_UPDATE_LEFT      (WM_USER + 1)   // wParam = TRUE/FALSE path stored in FDIFFUI_INFO.szFolderpath* or not, LPARAM = not used.
#define WM_UPDATE_RIGHT     (WM_USER + 2)   // same as above
#define WM_BROWSE_LEFT      (WM_USER + 3)
#define WM_BROWSE_RIGHT     (WM_USER + 4)

#define FSPEC_STATE_EMPTY           0
#define FSPEC_STATE_FILLED          1
#define FSPEC_STATE_TOUPDATE        2

extern HINSTANCE g_hMainInstance;


// Struct to hold info about the UI elements
typedef struct _FDiffUiInfo
{
    int iFSpecState_Left;
    int iFSpecState_Right;

    PDIRINFO pLeftDirInfo;
    PDIRINFO pRightDirInfo;

    WCHAR szFolderpathLeft[MAX_PATH];
    WCHAR szFolderpathRight[MAX_PATH];

    HWND hLvLeft;
    HWND hLvRight;
    HWND hStaticLeft;
    HWND hStaticRight;

}FDIFFUI_INFO;


// File local variables
static WCHAR* aszColumnNames[] = { L"Name", L"DType", L"Folder", L"MDate", L"Size" };
static int aiColumnWidthPercent[] = { 40, 10, 10, 25, 15 };
int (CALLBACK *pafnLvCompare[])(LPARAM, LPARAM, LPARAM) = { lvCmpName, lvCmpDupType, lvCmpPath, lvCmpDate, lvCmpSize };

// File local function prototypes
static BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo, _In_ BOOL fRecursive);
static BOOL UpdateDirInfo(_In_z_ PCWSTR pszFolderpath, _In_ PDIRINFO* ppDirInfo, _In_ BOOL fRecursive);
static BOOL CheckShowInvalidDirMBox(_In_ HWND hDlg, _In_ PCWSTR pszFolderpath);

// Function definitions

BOOL CALLBACK FolderDiffDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    static FDIFFUI_INFO uiInfo;

    switch(message)
	{
		case WM_INITDIALOG:
		{
            ZeroMemory(&uiInfo, sizeof(uiInfo));

            // Initialize the both the list views by adding the columns
            uiInfo.hLvLeft = GetDlgItem(hDlg, IDC_LIST_LEFT);
            uiInfo.hLvRight = GetDlgItem(hDlg, IDC_LIST_RIGHT);
            uiInfo.hStaticLeft = GetDlgItem(hDlg, IDC_STATIC_LEFT);
            uiInfo.hStaticRight = GetDlgItem(hDlg, IDC_STATIC_RIGHT);

            // Enable full-row selection instead of only the first column
            SendMessage(uiInfo.hLvLeft, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);
            SendMessage(uiInfo.hLvRight, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)LVS_EX_FULLROWSELECT);

            fChlGuiInitListViewColumns(uiInfo.hLvLeft, aszColumnNames, ARRAYSIZE(aszColumnNames), aiColumnWidthPercent);
            fChlGuiInitListViewColumns(uiInfo.hLvRight, aszColumnNames, ARRAYSIZE(aszColumnNames), aiColumnWidthPercent);

			return TRUE;
		}

        case WM_CLOSE:
		{
            if(uiInfo.pLeftDirInfo)
            {
                DestroyDirInfo(uiInfo.pLeftDirInfo);
                uiInfo.pLeftDirInfo = NULL;
            }

            if(uiInfo.pRightDirInfo)
            {
                DestroyDirInfo(uiInfo.pRightDirInfo);
                uiInfo.pRightDirInfo = NULL;
            }

			EndDialog(hDlg, 0);
			return TRUE;
		}

        case WM_SIZE:
        {
            break;
        }

		case WM_COMMAND:
		{
            if(wParam == IDOK)
            {
                // Check if enter was pressed in either of the edit controls and handle it
                HWND hFocusedItem = GetFocus();
                if(hFocusedItem == GetDlgItem(hDlg, IDC_EDIT_LEFT))
                {
                    SendMessage(hDlg, WM_UPDATE_LEFT, FALSE, (LPARAM)NULL);
                }
                else if(hFocusedItem == GetDlgItem(hDlg, IDC_EDIT_RIGHT))
                {
                    SendMessage(hDlg, WM_UPDATE_RIGHT, FALSE, (LPARAM)NULL);
                }

                return TRUE;
            }// if(wParam == IDOK)

			// handle commands from child controls
			switch(LOWORD(wParam))
			{
                case IDC_BTN_BRWS_LEFT:
                    if(SUCCEEDED(GetFolderToOpen(uiInfo.szFolderpathLeft)))
                    {
                        // Populate the edit control with this path
                        SendMessage(GetDlgItem(hDlg, IDC_EDIT_LEFT), WM_SETTEXT, 0, (LPARAM)uiInfo.szFolderpathLeft);
                    }
                    return TRUE;

                case IDC_BTN_BRWS_RIGHT:
                    if(SUCCEEDED(GetFolderToOpen(uiInfo.szFolderpathRight)))
                    {
                        // Populate the edit control with this path
                        SendMessage(GetDlgItem(hDlg, IDC_EDIT_RIGHT), WM_SETTEXT, 0, (LPARAM)uiInfo.szFolderpathRight);
                    }
                    return TRUE;

                case IDC_BTN_LOAD_LEFT:
                    if(GetTextFromEditControl(GetDlgItem(hDlg, IDC_EDIT_LEFT), 
                        uiInfo.szFolderpathLeft, ARRAYSIZE(uiInfo.szFolderpathLeft)))
                    {
                        if(uiInfo.szFolderpathLeft[0] != 0)
                        {
                            SendMessage(hDlg, WM_UPDATE_LEFT, FALSE, (LPARAM)NULL);
                        }
                        else if(SUCCEEDED(GetFolderToOpen(uiInfo.szFolderpathLeft)))
                        {
                            // Populate the edit control with this path and send message to update list view
                            SendMessage(GetDlgItem(hDlg, IDC_EDIT_LEFT), WM_SETTEXT, 0, (LPARAM)uiInfo.szFolderpathLeft);
                            SendMessage(hDlg, WM_UPDATE_LEFT, TRUE, (LPARAM)NULL);
                        }
                    }
                    return TRUE;

                case IDC_BTN_LOAD_RIGHT:
                    if(GetTextFromEditControl(GetDlgItem(hDlg, IDC_EDIT_RIGHT), 
                        uiInfo.szFolderpathRight, ARRAYSIZE(uiInfo.szFolderpathRight)))
                    {
                        if(uiInfo.szFolderpathRight[0] != 0)
                        {
                            SendMessage(hDlg, WM_UPDATE_RIGHT, FALSE, (LPARAM)NULL);
                        }
                        else if(SUCCEEDED(GetFolderToOpen(uiInfo.szFolderpathRight)))
                        {
                            // Populate the edit control with this path and send message to update list view
                            SendMessage(GetDlgItem(hDlg, IDC_EDIT_RIGHT), WM_SETTEXT, 0, (LPARAM)uiInfo.szFolderpathRight);
                            SendMessage(hDlg, WM_UPDATE_RIGHT, TRUE, (LPARAM)NULL);
                        }
                    }

                case IDC_BTN_DEL_LEFT:
                {
                    int nItemsSel;
                    PWSTR paszFiles;
                    if(GetSelectedLvItemsText(uiInfo.hLvLeft, &paszFiles, &nItemsSel, MAX_PATH))
                    {
                        DeleteFilesInDir(uiInfo.pLeftDirInfo, paszFiles, nItemsSel, uiInfo.pRightDirInfo);
                        free(paszFiles);
                        paszFiles = NULL;

                        // Update the list views
                        // TODO: This function destroys dir info and rebuilds it. Not necessary.
                        BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_LEFT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo, fRecursive);
                    }
                    return TRUE;
                }

                case IDC_BTN_DEL_RIGHT:
                {
                    int nItemsSel;
                    PWSTR paszFiles;
                    if(GetSelectedLvItemsText(uiInfo.hLvRight, &paszFiles, &nItemsSel, MAX_PATH))
                    {
                        DeleteFilesInDir(uiInfo.pRightDirInfo, paszFiles, nItemsSel, uiInfo.pLeftDirInfo);
                        free(paszFiles);
                        paszFiles = NULL;

                        // Update the list views
                        // TODO: This function destroys dir info and rebuilds it. Not necessary.
                        BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_LEFT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo, fRecursive);
                    }
                    return TRUE;
                }

                case IDC_BTN_DELALL_LEFT:
                    if(uiInfo.pLeftDirInfo != NULL)
                    {
                        BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_LEFT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;

                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pLeftDirInfo, uiInfo.pRightDirInfo);
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;

                        // Update right if it is already filled
                        if(uiInfo.iFSpecState_Right == FSPEC_STATE_FILLED)
                        {
                            uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        }
                        UpdateFileListViews(&uiInfo, fRecursive);
                    }
                    return TRUE;

                case IDC_BTN_DELALL_RIGHT:
                    if(uiInfo.pRightDirInfo != NULL)
                    {
                        BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_LEFT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;

                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pRightDirInfo, uiInfo.pLeftDirInfo);
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;

                        // Update left if it is already filled
                        if(uiInfo.iFSpecState_Left == FSPEC_STATE_FILLED)
                        {
                            uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        }
                        UpdateFileListViews(&uiInfo, fRecursive);
                    }
                    return TRUE;
			}

			break;
		}// WM_COMMAND

        case WM_NOTIFY:
        {
            switch(((LPNMHDR)lParam)->code)
            {
                case LVN_COLUMNCLICK:
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if(pnmv->hdr.idFrom == IDC_LIST_LEFT)
                    {
                        //NT_ASSERT(pnmv->iSubItem >= 0 && pnmv->iSubItem <= ARRAYSIZE(pafnLvCompare));
                        SendMessage(uiInfo.hLvLeft, LVM_SORTITEMSEX, (WPARAM)uiInfo.hLvLeft, (LPARAM)pafnLvCompare[pnmv->iSubItem]);
                    }
                    else if(pnmv->hdr.idFrom == IDC_LIST_RIGHT)
                    {
                        SendMessage(uiInfo.hLvRight, LVM_SORTITEMSEX, (WPARAM)uiInfo.hLvRight, (LPARAM)pafnLvCompare[pnmv->iSubItem]);
                    }
                    return TRUE;
            }

            break;
        }// WM_NOTIFY

        case WM_UPDATE_LEFT:
        {
            BOOL fRetVal = TRUE;
            if(wParam == FALSE)
            {
                fRetVal = GetTextFromEditControl(GetDlgItem(hDlg, IDC_EDIT_LEFT), 
                            uiInfo.szFolderpathLeft, ARRAYSIZE(uiInfo.szFolderpathLeft));
            }

            fRetVal &= CheckShowInvalidDirMBox(hDlg, uiInfo.szFolderpathLeft);
            if(fRetVal)
            {
                BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_LEFT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;
                uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                UpdateFileListViews(&uiInfo, fRecursive);
            }
            return TRUE;
        }

        case WM_UPDATE_RIGHT:
        {
            BOOL fRetVal = TRUE;
            if(wParam == FALSE)
            {
                fRetVal = GetTextFromEditControl(GetDlgItem(hDlg, IDC_EDIT_RIGHT), 
                            uiInfo.szFolderpathRight, ARRAYSIZE(uiInfo.szFolderpathRight));
            }

            fRetVal &= CheckShowInvalidDirMBox(hDlg, uiInfo.szFolderpathRight);
            if(fRetVal)
            {
                BOOL fRecursive = SendMessage(GetDlgItem(hDlg, IDC_CHECK_RIGHT), BM_GETCHECK, 0, (LPARAM)NULL) == BST_CHECKED ? TRUE : FALSE;
                uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                UpdateFileListViews(&uiInfo, fRecursive);
            }
            return TRUE;
        }

    }// switch(message)
	return FALSE;
}

BOOL UpdateDirInfo(_In_z_ PCWSTR pszFolderpath, _In_ PDIRINFO* ppDirInfo, _In_ BOOL fRecursive)
{
    //NT_ASSERT(ppDirInfo);

    if(*ppDirInfo != NULL)
    {
        DestroyDirInfo(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return BuildDirInfo(pszFolderpath, fRecursive, ppDirInfo);
}

BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo, _In_ BOOL fRecursive)
{
    //NT_ASSERT(pUiInfo);

    BOOL fRetVal = TRUE;

    if(pUiInfo->iFSpecState_Left == FSPEC_STATE_TOUPDATE)
    {
        // NT_ASSERT(pUiInfo->szFolderpathLeft);
        if(!UpdateDirInfo(pUiInfo->szFolderpathLeft, &pUiInfo->pLeftDirInfo, fRecursive))
        {
            goto error_return;
        }

        // Must update the right folder, if it is already filled, when updating the left.
        if(pUiInfo->iFSpecState_Right == FSPEC_STATE_FILLED)
        {
            //NT_ASSERT(pUiInfo->pRightDirInfo);
            ClearFilesDupFlag(pUiInfo->pRightDirInfo);
            if(!CompareDirsAndMarkFiles(pUiInfo->pLeftDirInfo, pUiInfo->pRightDirInfo))
            {
                goto error_return;
            }

            if(PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo))
            {
                WCHAR szStats[32];
                swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pRightDirInfo->nDirs, pUiInfo->pRightDirInfo->nFiles);
                SetWindowText(pUiInfo->hStaticRight, szStats);
            }
        }

        if(PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo))
        {
            pUiInfo->iFSpecState_Left = FSPEC_STATE_FILLED;

            WCHAR szStats[32];
            swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pLeftDirInfo->nDirs, pUiInfo->pLeftDirInfo->nFiles);
            SetWindowText(pUiInfo->hStaticLeft, szStats);
        }
    }
    else if(pUiInfo->iFSpecState_Right == FSPEC_STATE_TOUPDATE)
    {
        // NT_ASSERT(pUiInfo->szFolderpathRight);
        if(!UpdateDirInfo(pUiInfo->szFolderpathRight, &pUiInfo->pRightDirInfo, fRecursive))
        {
            goto error_return;
        }

        // Must update the left folder, if it is already filled, when updating the right.
        if(pUiInfo->iFSpecState_Left == FSPEC_STATE_FILLED)
        {
            //NT_ASSERT(pUiInfo->pLeftDirInfo);
            ClearFilesDupFlag(pUiInfo->pLeftDirInfo);
            if(!CompareDirsAndMarkFiles(pUiInfo->pRightDirInfo, pUiInfo->pLeftDirInfo))
            {
                goto error_return;
            }

            if(PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo))
            {
                WCHAR szStats[32];
                swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pLeftDirInfo->nDirs, pUiInfo->pLeftDirInfo->nFiles);
                SetWindowText(pUiInfo->hStaticLeft, szStats);
            }
        }

        if(PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo))
        {
            pUiInfo->iFSpecState_Right = FSPEC_STATE_FILLED;

            WCHAR szStats[32];
            swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pRightDirInfo->nDirs, pUiInfo->pRightDirInfo->nFiles);
            SetWindowText(pUiInfo->hStaticRight, szStats);
        }
    }

    return TRUE;

error_return:
    return FALSE;
}

static BOOL CheckShowInvalidDirMBox(_In_ HWND hDlg, _In_ PCWSTR pszFolderpath)
{
    BOOL fValidFolder = TRUE;

    WCHAR szMsg[MAX_PATH << 1];
    DWORD dwAttr = GetFileAttributes(pszFolderpath);
    if(dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        fValidFolder = FALSE;
        logerr(L"GetFileAttributes() failed for %s", pszFolderpath);
        swprintf_s(szMsg, ARRAYSIZE(szMsg), L"Directory \"%s\" does not exist or cannot be accessed.", pszFolderpath);
        MessageBox(hDlg, szMsg, L"Error", MB_OK | MB_ICONEXCLAMATION);
    }
    else if(!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        fValidFolder = FALSE;
        swprintf_s(szMsg, ARRAYSIZE(szMsg), L"\"%s\" is not a directory.", pszFolderpath);
        MessageBox(hDlg, szMsg, L"Error", MB_OK | MB_ICONEXCLAMATION);
    }
    return fValidFolder;
}
