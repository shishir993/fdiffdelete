
// Defines.h
// Contains common #defines, typedefs and data structures
// Shishir Bhat (http://www.shishirbhat.com)
// History
//      09/09/14 Refactor to store defs in individual headers.
//		08/04/2015 Make individual headers usable by clients
//

#ifndef CHL_DEFINES_H
#define CHL_DEFINES_H

#include <Windows.h>
#include <winnt.h>

// -------------------------------------------
// #defs and typedefs

#ifdef CHELPLIB_EXPORTS
#define DllExpImp __declspec( dllexport )
#else
#define DllExpImp __declspec( dllimport )
#endif // CHELPLIB_EXPORTS

// TODO: Is there a defined range for custom HRESULT codes?

// Custom error codes
#define CHLE_MUTEX_TIMEOUT  17010
#define CHLE_EMPTY_FILE     17011

#ifndef PCVOID
typedef PVOID const PCVOID;
#endif

// -------------------------------------------
// Structures

union _tagCHL_KEYDEF {
	PSTR pszKey;
	PWSTR pwszKey;
	int iKey;
	UINT uiKey;
	PVOID pvKey;
};

union _tagCHL_VALDEF {
	int iVal;
	UINT uiVal;
	double dVal;
	PSTR pszVal;    // Pointer to ANSI string(value is allocated on heap)
	PWSTR pwszVal;  // Pointer to wide string(value is allocated on heap)
	PVOID pvPtr;    // Stores a pointer(any type)(value is allocated on heap)
	PVOID pvUserObj;   // Pointer to user object(value is allocated on heap)
};

typedef struct CHL_KEY
{
	// Num. of bytes as size of key
	int iKeySize;

	// Storage for the key
	union _tagCHL_KEYDEF keyDef;

}CHL_KEY, *PCHL_KEY;

typedef struct CHL_VAL
{
	// Num. of bytes as size of val
	int iValSize;

	// Storage for the value
	union _tagCHL_VALDEF valDef;

}CHL_VAL, *PCHL_VAL;

// -------------------------------------------
// Enumerations

// Key Types
typedef enum
{
    // Invalid value type
    CHL_KT_START,

    // Signed 32bit integer
    CHL_KT_INT32,

    // Unsigned 32bit integer
    CHL_KT_UINT32,

    // A pointer whose pointed-to address is the key
    CHL_KT_POINTER,

    // Null-terminated ANSI string
    CHL_KT_STRING,

    // Null-terminated Unicode(wide char) string
    CHL_KT_WSTRING,

    // Invalid value type
    CHL_KT_END
}CHL_KEYTYPE;

// Value Types
typedef enum
{
    // Invalid value type
    CHL_VT_START,

    // Signed 32bit integer
    CHL_VT_INT32,

    // Unsigned 32bit integer
    CHL_VT_UINT32,

    // Pointer of any type(pointed-to address is the value stored)
    CHL_VT_POINTER,

    // Any user object(data structure).
    CHL_VT_USEROBJECT,

    // Null-terminated ANSI string
    CHL_VT_STRING,

    // Null-terminated Unicode(wide char) string
    CHL_VT_WSTRING,

    // Invalid value type
    CHL_VT_END
}CHL_VALTYPE;

#endif // CHL_DEFINES_H
