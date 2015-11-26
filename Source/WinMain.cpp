
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "common.h"
#include <io.h>
#include <fcntl.h>

#include "resource.h"
#include "DialogProc.h"

HINSTANCE g_hMainInstance;

HANDLE g_hStdOut = NULL;

static int iConInHandle = -1;
static int iConOutHandle = -1;
static int iConErrHandle = -1;

static BOOL CreateConsoleWindow();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR szCmdLine, int iCmdShow)
{
    DBG_UNREFERENCED_PARAMETER(szCmdLine);
    DBG_UNREFERENCED_PARAMETER(hPrevInstance);

	g_hMainInstance = hInstance;

    // Initialize common controls
	InitCommonControls();

    if(!CreateConsoleWindow())
    {
        MessageBox(NULL, L"Cannot create console window.", L"Warning", MB_OK | MB_ICONWARNING);
    }

    INT_PTR iptr = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DLG_FDIFF), NULL, FolderDiffDP);
    if(iptr == -1)
    {
        DWORD dw = GetLastError();
        MessageBox(NULL, L"Cannot create dialog box.", L"Error", MB_OK | MB_ICONERROR);
        DebugBreak();
    }
	
	return 0;
}

static BOOL CreateConsoleWindow()
{
    BOOL fError = TRUE;

    __try
    {
        if(AllocConsole())
        {
            // Thanks to: http://dslweb.nwnexus.com/~ast/dload/guicon.htm and MSDN
            FILE *fp = NULL;
            g_hStdOut = GetStdHandle(STD_INPUT_HANDLE);
            HANDLE hOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            HANDLE hErrorHandle = GetStdHandle(STD_ERROR_HANDLE);

            if(g_hStdOut == INVALID_HANDLE_VALUE || hOutputHandle == INVALID_HANDLE_VALUE ||
                hErrorHandle == INVALID_HANDLE_VALUE)
            {
                __leave;
            }

            if( (iConInHandle = _open_osfhandle((intptr_t)g_hStdOut, _O_RDONLY|_O_TEXT)) == -1 )
                __leave;

            fp = _fdopen( iConInHandle, "r" );
            *stdin = *fp;

            if( (iConOutHandle = _open_osfhandle((intptr_t)hOutputHandle, _O_APPEND|_O_TEXT)) == -1 )
                __leave;

            fp = _fdopen( iConOutHandle, "w+" );
            *stdout = *fp;

            if( (iConErrHandle = _open_osfhandle((intptr_t)hErrorHandle, _O_APPEND|_O_TEXT)) == -1 )
                __leave;

            fp = _fdopen( iConErrHandle, "w+" );
            *stderr = *fp;

            fError = FALSE;
        }
    }
    __finally
    {
        if(fError)
        {
			if (iConInHandle != -1) { _close(iConInHandle); iConInHandle = -1; }
			if (iConOutHandle != -1) { _close(iConOutHandle); iConOutHandle = -1; }
			if (iConErrHandle != -1) { _close(iConErrHandle); iConErrHandle = -1; }
			FreeConsole();
		}
    }

    return !fError;
}
