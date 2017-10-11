#pragma once

// ---------------------------------------------------
// The FDiffDelete Project
// Github: https://github.com/shishir993/fdiffdelete
// Author: Shishir Bhat
// The MIT License (MIT)
// Copyright (c) 2014
//

/**
 * Original work from http://c.learncodethehardway.org/book/ex20.html
 * Modified/additions to my liking
 */

//#define TURNOFF_LOGGING

#ifndef TURNOFF_LOGGING

void __logHelper(PCWSTR pszFmt, ...);
#define logtrace(M, ...)    __logHelper(L"[TRCE]  %s()+%d: " M L"\n", __FUNCTIONW__, __LINE__, ##__VA_ARGS__)
#define logerr(M, ...)      __logHelper(L"[ERRR]  %s()+%d: " M L"\n", __FUNCTIONW__, __LINE__, ##__VA_ARGS__)
#define logwarn(M, ...)     __logHelper(L"[WARN]  %s()+%d: " M L"\n", __FUNCTIONW__, __LINE__, ##__VA_ARGS__)

#else

#define logtrace(M, ...)
#define logerr(M, ...)
#define logwarn(M, ...)

#endif

#ifdef _DEBUG

#ifndef TURNOFF_LOGGING

#define logdbg(M, ...)      __logHelper(L"[DBG ]  %s()+%d: " M L"\n", __FUNCTIONW__, __LINE__, ##__VA_ARGS__)
#define loginfo(M, ...)     __logHelper(L"[INFO]  %s()+%d: " M L"\n", __FUNCTIONW__, __LINE__, ##__VA_ARGS__)

#else

#define logdbg(M, ...)
#define loginfo(M, ...)

#endif

#define DBG_MEMSET(mem, size) ( memset(mem, 0xCE, size) )

#else
#define logdbg(M, ...)
#define loginfo(M, ...)

#define NDBG_MEMSET(mem, size) ( memset(mem, 0x00, size) )
#define DBG_MEMSET(mem, size)   NDBG_MEMSET(mem, size)
#endif
