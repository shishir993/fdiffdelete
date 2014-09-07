#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "Common.h"

// SHA-1 hash is 160bits == 20bytes == 40 characters.
#define HASHLEN_SHA1    20
#define STRLEN_SHA1     ((HASHLEN_SHA1 << 1) + 1)

HRESULT HashFactoryInit(_Out_ HCRYPTPROV *phCrypt);
void HashFactoryDestroy(_In_ HCRYPTPROV hCrypt);

HRESULT CalculateSHA1(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash);
void HashValueToString(_In_bytecount_c_(HASHLEN_SHA1) PBYTE pbHash, _Inout_z_ PSTR pszHash);
