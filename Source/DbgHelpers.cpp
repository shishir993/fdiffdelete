
#include "Common.h"
#include "DbgHelpers.h"

extern HANDLE g_hStdOut;

#ifndef TURNOFF_LOGGING

static wchar_t s_szBuffer[1024];

void __logHelper(PCWSTR pszFmt, ...)
{
	if (g_hStdOut == NULL)
	{
		return;
	}

	va_list arg;
	va_start(arg, pszFmt);
	HRESULT hr = StringCchVPrintf(s_szBuffer, ARRAYSIZE(s_szBuffer), pszFmt, arg);
	if (SUCCEEDED(hr) || (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)))
	{
		WriteConsole(g_hStdOut, s_szBuffer, wcslen(s_szBuffer), NULL, NULL);
	}

}

#endif // TURNOFF_LOGGING
