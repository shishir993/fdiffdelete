#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include <windows.h>
#include <WinNT.h>
#include <wchar.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <CommCtrl.h>
#include "Assert.h"
#include "DbgHelpers.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// Custom error codes
#define CE_ASSERT_FAILED    0x666
