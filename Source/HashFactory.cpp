
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "HashFactory.h"
#include <WinCrypt.h>

HRESULT HashFactoryInit(_Out_ HCRYPTPROV *phCrypt)
{
    HRESULT hr = S_OK;

    *phCrypt = NULL;

    HCRYPTPROV hCrypt = NULL;
    if(CryptAcquireContext(
        &hCrypt,// handle out
        NULL,   // container
        NULL,   // provider
        PROV_DSS,           // provider type
        CRYPT_VERIFYCONTEXT))   // flags
    {
        *phCrypt = hCrypt;
    }
    else
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptAcquireContext() failed.");
    }
    return hr;
}

void HashFactoryDestroy(_In_ HCRYPTPROV hCrypt)
{
    SB_ASSERT(hCrypt != NULL);
    CryptReleaseContext(hCrypt, 0);
}

HRESULT CalculateSHA1(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash)
{
    SB_ASSERT(hCrypt != NULL);

    HRESULT hr = S_OK;
    HCRYPTHASH hHash = NULL;
	HANDLE hMapObj = NULL;
	HANDLE hMapView = NULL;
	DWORD dwHashSize = HASHLEN_SHA1;
	LARGE_INTEGER fileSize = {};

	if (!CryptCreateHash(hCrypt, CALG_SHA1, NULL, 0, &hHash))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		logerr(L"CryptCreateHash() failed");
		goto fend;
	}
        
	hr = CHL_GnCreateMemMapOfFile(hFile, PAGE_READONLY, &hMapObj, &hMapView);
	if (FAILED(hr))
	{
		logerr(L"Failed to mem-map file, hr: %x", hr);
		goto fend;
	}

	if (!GetFileSizeEx(hFile, &fileSize))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		logerr(L"GetFileSizeEx failed, hr: %x", hr);
		goto fend;
	}
	if (MAXDWORD < fileSize.QuadPart)
	{
		hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
		logerr(L"File too big");
		goto fend;
	}

	if (!CryptHashData(hHash, (PBYTE)hMapView, fileSize.LowPart, 0))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		logerr(L"CryptHashData failed, hr: %x", hr);
		goto fend;
	}

	if (CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashSize, 0))
	{
		hr = HRESULT_FROM_WIN32(GetLastError());
		logerr(L"CryptGetHashParam failed, hr: %x");
		goto fend;
	}

#ifdef _DEBUG
    CHAR szHash[STRLEN_SHA1];
    HashValueToString(pbHash, szHash);
    wprintf(L"Hash string = [%S]\n", szHash);
#endif

fend:
	if (hHash != NULL)
	{
		CryptDestroyHash(hHash);
		hHash = NULL;
	}

	if (hMapView != NULL)
	{
		UnmapViewOfFile(hMapView);
		hMapView = NULL;
	}

	if (hMapObj != NULL)
	{
		CloseHandle(hMapObj);
		hMapObj = NULL;
	}
	
    return hr;
}

void HashValueToString(_In_bytecount_c_(HASHLEN_SHA1) PBYTE pbHash, _Inout_z_ PSTR pszHashValue)
{
    for(int i = 0; i < HASHLEN_SHA1; ++i)
    {
        sprintf_s(pszHashValue + (i<<1), STRLEN_SHA1 - (i<<1), "%02x", pbHash[i]);
    }
    pszHashValue[STRLEN_SHA1-1] = 0;
}
