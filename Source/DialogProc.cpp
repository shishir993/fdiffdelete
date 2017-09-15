
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "DialogProc.h"
#include "resource.h"
#include "Hashtable.h"
#include "UIHelpers.h"
#include "DirectoryWalker_Interface.h"

enum {
	WM_DIFF = WM_USER + 1,
	WM_BROWSE_LEFT,
	WM_BROWSE_RIGHT
};

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
static int aiColumnWidthPercent[] =
{ 
    40,     // Name
    10,     // DupType
    20,     // Folder
    15,     // Modified Date
    15      // Size
};
int (CALLBACK *pafnLvCompare[])(LPARAM, LPARAM, LPARAM) = { lvCmpName, lvCmpDupType, lvCmpPath, lvCmpDate, lvCmpSize };

// File local function prototypes
static BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo, _In_ BOOL fRecursive, _In_ BOOL fCompareHashes);
static BOOL UpdateDirInfo(
    _In_z_ PCWSTR pszFolderpath,
    _In_ PDIRINFO* ppDirInfo,
    _In_ BOOL fRecursive,
    _In_ BOOL fCompareHashes);

static BOOL CheckInvalidDir(_In_ HWND hDlg, _In_ PCWSTR pszFolderpath);

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

			CHL_GuiInitListViewColumns(uiInfo.hLvLeft, aszColumnNames, ARRAYSIZE(aszColumnNames), aiColumnWidthPercent);
			CHL_GuiInitListViewColumns(uiInfo.hLvRight, aszColumnNames, ARRAYSIZE(aszColumnNames), aiColumnWidthPercent);

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

            FileInfoDestroy();

			EndDialog(hDlg, 0);
			return TRUE;
		}

        case WM_SIZE:
        {
            break;
        }

		case WM_COMMAND:
		{
			// handle commands from child controls
			switch(LOWORD(wParam))
			{
                case IDM_ENABLEHASHCOMPARE:
                {
                    HMENU hMenu = GetMenu(hDlg);
                    if(IsMenuItemChecked(hMenu, IDM_ENABLEHASHCOMPARE))
                    {
                        // Remove check mark
                        CheckMenuItem(hMenu, IDM_ENABLEHASHCOMPARE, MF_BYCOMMAND | MF_UNCHECKED);
                    }
                    else
                    {
                        CheckMenuItem(hMenu, IDM_ENABLEHASHCOMPARE, MF_BYCOMMAND | MF_CHECKED);
                    }
                    return 0;
                }

                case IDM_ENABLERECURSIVECOMPARE:
                {
                    HMENU hMenu = GetMenu(hDlg);
                    if(IsMenuItemChecked(hMenu, IDM_ENABLERECURSIVECOMPARE))
                    {
                        // Remove check mark
                        CheckMenuItem(hMenu, IDM_ENABLERECURSIVECOMPARE, MF_BYCOMMAND | MF_UNCHECKED);
                    }
                    else
                    {
                        CheckMenuItem(hMenu, IDM_ENABLERECURSIVECOMPARE, MF_BYCOMMAND | MF_CHECKED);
                    }
                    return 0;
                }

                case IDM_ABOUT:
                {
                    break;
                }

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

				case IDC_BTN_DIFF:
				{
					if (uiInfo.szFolderpathLeft[0] != 0 && uiInfo.szFolderpathRight[0] != 0)
					{
						SendMessage(hDlg, WM_DIFF, FALSE, (LPARAM)NULL);
						return TRUE;
					}
					return FALSE;
				}

                case IDC_BTN_DEL_LEFT:
                {
					int nItemsSel;
                    BOOL fSucceeded = FALSE;

					if (uiInfo.pLeftDirInfo == NULL)
					{
						return FALSE;
					}

                    if(uiInfo.pLeftDirInfo->fHashCompare)
                    {
                        PFILEINFO *ppFiles;
                        if(GetSelectedLvItemsText_Hash(uiInfo.hLvLeft, &ppFiles, &nItemsSel))
                        {
                            fSucceeded = DeleteFilesInDir(uiInfo.pLeftDirInfo, ppFiles, nItemsSel, uiInfo.pRightDirInfo);
                            free(ppFiles);
                            ppFiles = NULL;
                        }
                    }
                    else
                    {
                        PWSTR paszFiles;
                        if(GetSelectedLvItemsText(uiInfo.hLvLeft, &paszFiles, &nItemsSel, MAX_PATH))
                        {
                            fSucceeded = DeleteFilesInDir(uiInfo.pLeftDirInfo, paszFiles, nItemsSel, uiInfo.pRightDirInfo);
                            free(paszFiles);
                            paszFiles = NULL;
                        }
                    }

                    if(fSucceeded)
                    {
                        // Update the list views
                        // TODO: This function destroys dir info and rebuilds it. Not necessary.
                        BOOL fRecursive = IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLERECURSIVECOMPARE);
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo, fRecursive, IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLEHASHCOMPARE));
                    }
                    return TRUE;
                }

                case IDC_BTN_DEL_RIGHT:
                {
					int nItemsSel;
                    BOOL fSucceeded = FALSE;

					if (uiInfo.pRightDirInfo == NULL)
					{
						return FALSE;
					}

                    if(uiInfo.pRightDirInfo->fHashCompare)
                    {
                        PFILEINFO *ppFiles;
                        if(GetSelectedLvItemsText_Hash(uiInfo.hLvRight, &ppFiles, &nItemsSel))
                        {
                            fSucceeded = DeleteFilesInDir(uiInfo.pRightDirInfo, ppFiles, nItemsSel, uiInfo.pLeftDirInfo);
                            free(ppFiles);
                            ppFiles = NULL;
                        }
                    }
                    else
                    {
                        PWSTR paszFiles;
                        if(GetSelectedLvItemsText(uiInfo.hLvRight, &paszFiles, &nItemsSel, MAX_PATH))
                        {
                            fSucceeded = DeleteFilesInDir(uiInfo.pRightDirInfo, paszFiles, nItemsSel, uiInfo.pLeftDirInfo);
                            free(paszFiles);
                            paszFiles = NULL;
                        }
                    }

                    if(fSucceeded)
                    {
                        // Update the list views
                        // TODO: This function destroys dir info and rebuilds it. Not necessary.
                        BOOL fRecursive = IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLERECURSIVECOMPARE);
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        UpdateFileListViews(&uiInfo, fRecursive, IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLEHASHCOMPARE));
                    }
                    return TRUE;
                }

                case IDC_BTN_DELALL_LEFT:
                    if(uiInfo.pLeftDirInfo != NULL)
                    {
                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pLeftDirInfo, uiInfo.pRightDirInfo);
                        uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;

                        // Update right if it is already filled
                        if(uiInfo.iFSpecState_Right == FSPEC_STATE_FILLED)
                        {
                            uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
                        }

                        BOOL fRecursive = IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLERECURSIVECOMPARE);
                        UpdateFileListViews(&uiInfo, fRecursive, IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLEHASHCOMPARE));
                    }
                    return TRUE;

                case IDC_BTN_DELALL_RIGHT:
                    if(uiInfo.pRightDirInfo != NULL)
                    {
                        BOOL fRecursive = IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLERECURSIVECOMPARE);

                        // NULL check for the second parameter is in the callee
                        DeleteDupFilesInDir(uiInfo.pRightDirInfo, uiInfo.pLeftDirInfo);
                        uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;

                        // Update left if it is already filled
                        if(uiInfo.iFSpecState_Left == FSPEC_STATE_FILLED)
                        {
                            uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
                        }
                        UpdateFileListViews(&uiInfo, fRecursive, IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLEHASHCOMPARE));
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
				{
					LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
					if (pnmv->hdr.idFrom == IDC_LIST_LEFT)
					{
						SB_ASSERT(pnmv->iSubItem >= 0 && pnmv->iSubItem <= ARRAYSIZE(pafnLvCompare));
						SendMessage(uiInfo.hLvLeft, LVM_SORTITEMSEX, (WPARAM)uiInfo.hLvLeft, (LPARAM)pafnLvCompare[pnmv->iSubItem]);
					}
					else if (pnmv->hdr.idFrom == IDC_LIST_RIGHT)
					{
						SendMessage(uiInfo.hLvRight, LVM_SORTITEMSEX, (WPARAM)uiInfo.hLvRight, (LPARAM)pafnLvCompare[pnmv->iSubItem]);
					}
					return TRUE;
				}
            }

            break;
        }// WM_NOTIFY

		case WM_DIFF:
		{
			WCHAR szMessage[128] = {};
			if (!CheckInvalidDir(hDlg, uiInfo.szFolderpathLeft))
			{
				wcscpy_s(szMessage, L"Left folder does not exist or is inaccessible. ");
			}
			
			if (!CheckInvalidDir(hDlg, uiInfo.szFolderpathRight))
			{
				wcscat_s(szMessage, L"Right folder does not exist or is inaccessible.");
			}

			if (szMessage[0] == 0)
			{
				BOOL fRecursive = IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLERECURSIVECOMPARE);
				uiInfo.iFSpecState_Left = FSPEC_STATE_TOUPDATE;
				uiInfo.iFSpecState_Right = FSPEC_STATE_TOUPDATE;
				UpdateFileListViews(&uiInfo, fRecursive, IsMenuItemChecked(GetMenu(hDlg), IDM_ENABLEHASHCOMPARE));
			}
			else
			{
				MessageBox(hDlg, szMessage, L"Error", MB_OK | MB_ICONEXCLAMATION);
			}
			return TRUE;
		}

    }// switch(message)
	return FALSE;
}

static BOOL UpdateDirInfo(
    _In_z_ PCWSTR pszFolderpath,
    _In_ PDIRINFO* ppDirInfo,
    _In_ BOOL fRecursive,
    _In_ BOOL fCompareHashes)
{
    SB_ASSERT(ppDirInfo);

    if(*ppDirInfo != NULL)
    {
        DestroyDirInfo(*ppDirInfo);
        *ppDirInfo = NULL;
    }

    return BuildDirInfo(pszFolderpath, fRecursive, fCompareHashes, ppDirInfo);
}

BOOL UpdateFileListViews(_In_ FDIFFUI_INFO *pUiInfo, _In_ BOOL fRecursive, _In_ BOOL fCompareHashes)
{
    SB_ASSERT(pUiInfo);

    if(pUiInfo->iFSpecState_Left == FSPEC_STATE_TOUPDATE)
    {
        SB_ASSERT(pUiInfo->szFolderpathLeft);
        if(!UpdateDirInfo(pUiInfo->szFolderpathLeft, &pUiInfo->pLeftDirInfo, fRecursive, fCompareHashes))
        {
            goto error_return;
        }

        // Must update the right folder, if it is already filled, when updating the left.
        if(pUiInfo->iFSpecState_Right == FSPEC_STATE_FILLED)
        {
            SB_ASSERT(pUiInfo->pRightDirInfo);
            ClearFilesDupFlag(pUiInfo->pRightDirInfo);
            if(!CompareDirsAndMarkFiles(pUiInfo->pLeftDirInfo, pUiInfo->pRightDirInfo))
            {
                goto error_return;
            }

            if(PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo, fCompareHashes))
            {
                WCHAR szStats[32];
                swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pRightDirInfo->nDirs, pUiInfo->pRightDirInfo->nFiles);
                SetWindowText(pUiInfo->hStaticRight, szStats);
            }
        }

        if(PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo, fCompareHashes))
        {
            pUiInfo->iFSpecState_Left = FSPEC_STATE_FILLED;

            WCHAR szStats[32];
            swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pLeftDirInfo->nDirs, pUiInfo->pLeftDirInfo->nFiles);
            SetWindowText(pUiInfo->hStaticLeft, szStats);
        }
    }
    
	if(pUiInfo->iFSpecState_Right == FSPEC_STATE_TOUPDATE)
    {
        SB_ASSERT(pUiInfo->szFolderpathRight);
        if(!UpdateDirInfo(pUiInfo->szFolderpathRight, &pUiInfo->pRightDirInfo, fRecursive, fCompareHashes))
        {
            goto error_return;
        }

        // Must update the left folder, if it is already filled, when updating the right.
        if(pUiInfo->iFSpecState_Left == FSPEC_STATE_FILLED)
        {
            SB_ASSERT(pUiInfo->pLeftDirInfo);
            ClearFilesDupFlag(pUiInfo->pLeftDirInfo);
            if(!CompareDirsAndMarkFiles(pUiInfo->pRightDirInfo, pUiInfo->pLeftDirInfo))
            {
                goto error_return;
            }

            if(PopulateFileList(pUiInfo->hLvLeft, pUiInfo->pLeftDirInfo, fCompareHashes))
            {
                WCHAR szStats[32];
                swprintf_s(szStats, ARRAYSIZE(szStats), L"%d folders, %d files.", pUiInfo->pLeftDirInfo->nDirs, pUiInfo->pLeftDirInfo->nFiles);
                SetWindowText(pUiInfo->hStaticLeft, szStats);
            }
        }

        if(PopulateFileList(pUiInfo->hLvRight, pUiInfo->pRightDirInfo, fCompareHashes))
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

static BOOL CheckInvalidDir(_In_ HWND hDlg, _In_ PCWSTR pszFolderpath)
{
    BOOL fValidFolder = TRUE;

    DWORD dwAttr = GetFileAttributes(pszFolderpath);
    if(dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        fValidFolder = FALSE;
        logerr(L"GetFileAttributes() failed for %s", pszFolderpath);
    }
    else if(!(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        fValidFolder = FALSE;
    }
    return fValidFolder;
}
