
#include "Assert.h"

void SbAssert(const char* pszFile, UINT uLine)
{
    // ensure that all buffers are written out first
    fflush(NULL);
    fprintf(stderr, "Assertion failed in %s at Line %u\n", pszFile, uLine);
    fflush(stderr);

    // Adding a hard breakpoint because:
    // In GUI programs, there is usually no console window open to display the assertion failure.
    // Even when a GUI program explicitly opens a console window, exiting the program causes
    // the console also to be closed immediately. So we cannot see where the assertion failed.

    if (IsDebuggerPresent())
    {
        fprintf(stderr, "Debugger is present. Causing a breakpoint exception.\n");
        __asm int 3
    }

    exit(CE_ASSERT_FAILED);
}
