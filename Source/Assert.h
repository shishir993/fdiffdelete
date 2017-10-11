
#ifndef _ASSERT_H
#define _ASSERT_H

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "Common.h"

#ifdef _DEBUG
void SbAssert(const char* psFile, UINT uLine);
#define SB_ASSERT(x)    \
        if(x) {}        \
        else            \
        SbAssert(__FILE__, __LINE__)
#else
#define SB_ASSERT(x)
#endif

#endif //_ASSERT_H
