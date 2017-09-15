#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "Common.h"

int StringSizeBytes(_In_ PCWSTR pwsz);
BOOL IsFileFolderBanned(_In_z_ PWSTR pszFilename, _In_ int nMaxChars);
