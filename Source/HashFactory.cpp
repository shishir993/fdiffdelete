
// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

#include "HashFactory.h"
#include <WinCrypt.h>

#define MBYTES  (1024 * 1024)

static HRESULT _HashFileOneShot(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _In_ DWORD cbFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash);
static HRESULT _HashFilePieceMeal(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _In_ UINT64 cbFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash);


HRESULT HashFactoryInit(_Out_ HCRYPTPROV *phCrypt)
{
    HRESULT hr = S_OK;

    *phCrypt = NULL;

    HCRYPTPROV hCrypt = NULL;
    if (CryptAcquireContext(
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
    LARGE_INTEGER fileSize = {};

    if (!GetFileSizeEx(hFile, &fileSize))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"GetFileSizeEx failed, hr: %x", hr);
        goto fend;
    }
    
    if ((200 * MBYTES) <= fileSize.QuadPart)
    {
        hr = _HashFilePieceMeal(hCrypt, hFile, fileSize.QuadPart, pbHash);
    }
    else
    {
        hr = _HashFileOneShot(hCrypt, hFile, fileSize.LowPart, pbHash);
    }

    if (SUCCEEDED(hr))
    {
#ifdef _DEBUG
        CHAR szHash[STRLEN_SHA1];
        HashValueToString(pbHash, szHash);
        logdbg(L"Hash string = [%S]", szHash);
#endif
    }

fend:
    return hr;
}

void HashValueToString(_In_bytecount_c_(HASHLEN_SHA1) PBYTE pbHash, _Inout_z_ PSTR pszHashValue)
{
    for (int i = 0; i < HASHLEN_SHA1; ++i)
    {
        sprintf_s(pszHashValue + (i << 1), STRLEN_SHA1 - (i << 1), "%02x", pbHash[i]);
    }
    pszHashValue[STRLEN_SHA1 - 1] = 0;
}

HRESULT _HashFileOneShot(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _In_ DWORD cbFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash)
{
    HRESULT hr = S_OK;
    HCRYPTHASH hHash = NULL;
    HANDLE hMapObj = NULL;
    HANDLE hMapView = NULL;
    DWORD dwHashSize = HASHLEN_SHA1;

    hr = CHL_GnCreateMemMapOfFile(hFile, PAGE_READONLY, &hMapObj, &hMapView);
    if (FAILED(hr))
    {
        logerr(L"Failed to mem-map file, hr: %x", hr);
        goto fend;
    }

    if (!CryptCreateHash(hCrypt, CALG_SHA1, NULL, 0, &hHash))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptCreateHash() failed");
        goto fend;
    }

    if (!CryptHashData(hHash, (PBYTE)hMapView, cbFile, 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptHashData failed, hr: %x", hr);
        goto fend;
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashSize, 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptGetHashParam failed, hr: %x", hr);
        goto fend;
    }

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

HRESULT _HashFilePieceMeal(_In_ HCRYPTPROV hCrypt, _In_ HANDLE hFile, _In_ UINT64 cbFile, _Out_bytecap_c_(HASHLEN_SHA1) PBYTE pbHash)
{
    HRESULT hr = S_OK;
    void* pBuffer = NULL;
    UINT64 cbRemaining = cbFile;
    const DWORD cbBuf = 16 * MBYTES;

    HCRYPTHASH hHash = NULL;
    DWORD dwHashSize = HASHLEN_SHA1;

    hr = CHL_MmAlloc(&pBuffer, cbBuf, NULL);
    if (FAILED(hr))
    {
        goto fend;
    }

    if (!CryptCreateHash(hCrypt, CALG_SHA1, NULL, 0, &hHash))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptCreateHash() failed");
        goto fend;
    }

    while (0 < cbRemaining)
    {
        DWORD cbRead = 0;
        DWORD cbToRead = (DWORD)(min(cbRemaining, cbBuf));
        if (!ReadFile(hFile, pBuffer, cbToRead, &cbRead, NULL))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            logerr(L"ReadFile failed, hr: %x", hr);
            goto fend;
        }

        if (cbRead != cbToRead)
        {
            hr = E_UNEXPECTED;
            goto fend;
        }

        cbRemaining -= cbRead;

        if (!CryptHashData(hHash, (LPCBYTE)pBuffer, cbRead, 0))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            logerr(L"CryptHashData failed, hr: %x", hr);
            goto fend;
        }
    }

    if (!CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashSize, 0))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptGetHashParam failed, hr: %x", hr);
        goto fend;
    }

fend:
    if (hHash != NULL)
    {
        CryptDestroyHash(hHash);
        hHash = NULL;
    }
    CHL_MmFree(&pBuffer);
    return hr;
}
