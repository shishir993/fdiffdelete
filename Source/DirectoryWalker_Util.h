#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "Common.h"

void _ConvertToAscii(_In_ PCWSTR pwsz, _Out_ char* psz);
BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars);
