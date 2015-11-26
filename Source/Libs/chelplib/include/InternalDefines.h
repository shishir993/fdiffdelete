
// Defines.h
// Contains common #defines, typedefs and data structures
// Shishir Bhat (http://www.shishirbhat.com)
// History
//      09/18/2014 Standardize keys and value types
//		08/04/2015 Make individual headers usable by clients
//

#ifndef CHL_INT_DEFINES_H
#define CHL_INT_DEFINES_H

#include "CommonInclude.h"
#include "Assert.h"
#include "DbgHelpers.h"
#include "Helpers.h"
#include "Defines.h"

// -------------------------------------------
// Functions internal only

HRESULT _CopyKeyIn(_In_ PCHL_KEY pChlKey, _In_ CHL_KEYTYPE keyType, _In_ PCVOID pvKey, _Inout_opt_ int iKeySize);
HRESULT _CopyKeyOut(_In_ PCHL_KEY pChlKey, _In_ CHL_KEYTYPE keyType, _Inout_ PVOID pvKeyOut, _In_ BOOL fGetPointerOnly);
BOOL _IsDuplicateKey(_In_ PCHL_KEY pChlLeftKey, _In_ PCVOID pvRightKey, _In_ CHL_KEYTYPE keyType, _In_ int iKeySize);
void _DeleteKey(_In_ PCHL_KEY pChlKey, _In_ CHL_KEYTYPE keyType);
HRESULT _GetKeySize(_In_ PVOID pvKey, _In_ CHL_KEYTYPE keyType, _Inout_ PINT piKeySize);
HRESULT _EnsureSufficientKeyBuf(
	_In_ PCHL_KEY pChlKey,
	_In_ int iSpecBufSize,
	_Inout_opt_ PINT piReqBufSize);

HRESULT _CopyValIn(_In_ PCHL_VAL pChlVal, _In_ CHL_VALTYPE valType, _In_ PCVOID pvVal, _Inout_opt_ int iValSize);
HRESULT _CopyValOut(_In_ PCHL_VAL pChlVal, _In_ CHL_VALTYPE valType, _Inout_ PVOID pvValOut, _In_ BOOL fGetPointerOnly);
BOOL _IsDuplicateVal(_In_ PCHL_VAL pLeftVal, _In_ PCVOID pRightVal, _In_ CHL_VALTYPE valType, _In_ int iValSize);
void _DeleteVal(_In_ PCHL_VAL pChlVal, _In_ CHL_VALTYPE valType);
HRESULT _GetValSize(_In_ PVOID pvVal, _In_ CHL_VALTYPE valType, _Inout_ PINT piValSize);
HRESULT _EnsureSufficientValBuf(
	_In_ PCHL_VAL pChlVal,
	_In_ int iSpecBufSize,
	_Inout_opt_ PINT piReqBufSize);

#endif // CHL_INT_DEFINES_H
