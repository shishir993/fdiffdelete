
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

}FDIFFUI_INFO;


// File local variables
static WCHAR* aszColumnNames[] = { L"Name", L"DType", L"MDate", L"Size" };
static int aiColumnWidthPercent[] = { 50, 10, 25, 15 };
int (CALLBACK *pafnLvCompare[4])(LPARAM, LPARAM, LPARAM) = { lvCmpName, lvCmpDupType, lvCmpDate, lvCmpSize };

// File local function prototypes
static BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo);
static BOOL UpdateDirInfo(_In_z_ PCWSTR pszFolderpath, _In_ PDIRINFO* ppDirInfo);


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
                        SendMessage(hDlg, WM_UPDATE_LEFT, TRUE, (LPARAM)NULL);
                    }

                    return TRUE;

                case IDC_BTN_BRWS_RIGHT:
                    if(SUCCEEDED(GetFolderToOpen(uiInfo.szFolderpathRight)))
                    {
                        // Populate the edit control with this path
                        SendMessage(GetDlgItem(hDlg, IDC_EDIT_RIGHT), WM_SETTEXT, 0, (LPARAM)uiInfo.szFolderpathRight);
                        SendMessage(hDlg, WM_UPDATE_RIGHT, TRUE, (LPARAM)NULL);
                    }

                    return TRUE;

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
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo);
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
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo);
                    }
                    return TRUE;
                }

                case IDC_BTN_DELALL_LEFT:
                    if(uiInfo.pLeftDirInfo != NULL)
                    {
                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pLeftDirInfo, uiInfo.pRightDirInfo);
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo);
                    }
                    return TRUE;

                case IDC_BTN_DELALL_RIGHT:
                    if(uiInfo.pRightDirInfo != NULL)
                    {
                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pRightDirInfo, uiInfo.pLeftDirInfo);
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo);
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

            if(fRetVal)
            {
                BuildDirTree(uiInfo.szFolderpathLeft, &uiInfo.pLeftDirInfo);
                PrintDirTree(uiInfo.pLeftDirInfo);

                //uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                //UpdateFileListViews(&uiInfo);
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

            if(fRetVal)
            {
                uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                UpdateFileListViews(&uiInfo);
            }
            return TRUE;
        }

    }// switch(message)
	return FALSE;
}

BOOL UpdateDirInfo(_In_z_ PCWSTR pszFolderpath, _In_ PDIRINFO* ppDirInfo)
{
    //NT_ASSERT(ppDirInfo);

    if(*ppDirInfo != NULL)
    {
        DestroyDirInfo(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return BuildDirInfo(pszFolderpath, FALSE, ppDirInfo);
}

BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo)
{
    //NT_ASSERT(pUiInfo);

    BOOL fRetVal = TRUE;

    if(pUiInfo->iFSpecState_Left == FSPEC_STATE_TOUPDATE)
    {
        // NT_ASSERT(pUiInfo->szFolderpathLeft);
        if(!UpdateDirInfo(pUiInfo->szFolderpathLeft, &pUiInfo->pLeftDirInfo))
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

            PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo);
        }

        if(PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo))
        {
            pUiInfo->iFSpecState_Left = FSPEC_STATE_FILLED;
        }
    }
    else if(pUiInfo->iFSpecState_Right == FSPEC_STATE_TOUPDATE)
    {
        // NT_ASSERT(pUiInfo->szFolderpathRight);
        if(!UpdateDirInfo(pUiInfo->szFolderpathRight, &pUiInfo->pRightDirInfo))
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

            PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo);
        }

        if(PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo))
        {
            pUiInfo->iFSpecState_Right = FSPEC_STATE_FILLED;
        }
    }
    

    return TRUE;

error_return:
    return FALSE;
}
