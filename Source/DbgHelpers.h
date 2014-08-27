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

#define TURNOFF_LOGGING

#ifndef TURNOFF_LOGGING

#define clean_errno() (errno == 0 ? L"None" : _wcserror(errno))
#define logtrace(M, ...)    fwprintf(stderr, L"[TRACE] %s(): " M L"\n", __FUNCTIONW__, ##__VA_ARGS__)
#define logerr(M, ...)      fwprintf(stderr, L"[ERROR] %s(): " M L"(errno: %s, LastError: %u)\n", __FUNCTIONW__, ##__VA_ARGS__, clean_errno(), GetLastError())
#define logwarn(M, ...)     fwprintf(stderr, L"[WARN]  %s(): " M L"\n", __FUNCTIONW__, ##__VA_ARGS__)

#else

#define logtrace(M, ...)
#define logerr(M, ...)
#define logwarn(M, ...)

#endif

#ifdef _DEBUG

#ifndef TURNOFF_LOGGING

    #define logdbg(M, ...)      fwprintf(stdout, L"[DEBUG] %s(): " M L"\n", __FUNCTIONW__, ##__VA_ARGS__)
    #define loginfo(M, ...)     fwprintf(stderr, L"[INFO]  %s(): " M L"\n", __FUNCTIONW__, ##__VA_ARGS__)

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
