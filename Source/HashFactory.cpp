
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
    if(CryptCreateHash(hCrypt, CALG_SHA1, NULL, 0, &hHash))
    {
        HANDLE hMapObj, hMapView;
        if(SUCCEEDED(CHL_GnCreateMemMapOfFile(hFile, PAGE_READONLY, &hMapObj, &hMapView)))
        {
            ULARGE_INTEGER ullFileSize;
            ullFileSize.LowPart = GetFileSize(hFile, &ullFileSize.HighPart);
            if(ullFileSize.LowPart != INVALID_FILE_SIZE)
            {
                if(CryptHashData(hHash, (PBYTE)hMapView, ullFileSize.LowPart, 0))
                {
                    DWORD dwHashSize = HASHLEN_SHA1;
                    if(CryptGetHashParam(hHash, HP_HASHVAL, pbHash, &dwHashSize, 0))
                    {
#ifdef _DEBUG
                        CHAR szHash[STRLEN_SHA1];
                        HashValueToString(pbHash, szHash);
                        wprintf(L"Hash string = [%S]\n", szHash);
#endif
                    }
                    else
                    {
                        hr = HRESULT_FROM_WIN32(GetLastError());
                        logerr(L"CryptGetHashParam() failed.");

                    }
                }
                else
                {
                    hr = HRESULT_FROM_WIN32(GetLastError());
                    logerr(L"CryptHashData() failed.");
                }
            }
            else
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                logerr(L"GetFileSize().");
            }
            CloseHandle(hMapObj);
        }
        else
        {
            hr = E_FAIL;
            logerr(L"Cannot create memory map.");
        }
            
        CryptDestroyHash(hHash);
    }
    else
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        logerr(L"CryptCreateHash() failed");
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
