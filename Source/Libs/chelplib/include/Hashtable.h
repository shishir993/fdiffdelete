// CHelpLib.h
// Exported symbols from CHelpLib DLL
// Shishir Bhat (http://www.shishirbhat.com)
// History
//      Unknown history!
//      09/09/14 Refactor to store defs in individual headers.
//      09/12/14 Naming convention modifications
//

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#ifdef __cplusplus
extern "C" {  
#endif

#include "Defines.h"
#include "MemFunctions.h"

// hashtable node
typedef struct _hashTableNode {
    BOOL fOccupied;
    CHL_KEY chlKey;
    CHL_VAL chlVal;
    struct _hashTableNode *pnext;
}HT_NODE;

// Structure that defines the iterator for the hashtable
// Callers can use this to iterate through the hashtable
// and get all (key,value) pairs one-by-one
typedef struct _hashtableIterator {
    int opType;
    int nCurIndex;              // current position in the main bucket
    HT_NODE *phtCurNodeInList;  // current position in the sibling list
}CHL_HT_ITERATOR;

// hashtable itself
typedef struct _hashtable {
    CHL_KEYTYPE keyType;
    CHL_VALTYPE valType;
    BOOL fValIsInHeap;
    HT_NODE *phtNodes;
    int nTableSize;
	CRITICAL_SECTION csLock;

    // Access methods
    HRESULT (*Destroy)(struct _hashtable* phtable);

    HRESULT (*Insert)(
        struct _hashtable* phtable, 
        PCVOID pvkey, 
        int iKeySize, 
        PVOID pvVal, 
        int iValSize);

    HRESULT (*Find)(
        struct _hashtable* phtable, 
        PCVOID pvkey, 
        int iKeySize, 
        PVOID pvVal, 
        PINT pvalsize,
        BOOL fGetPointerOnly);

    HRESULT (*Remove)(struct _hashtable* phtable, PCVOID pvkey, int iKeySize);

    HRESULT (*InitIterator)(CHL_HT_ITERATOR *pItr);
    HRESULT (*GetNext)(
        struct _hashtable* phtable, 
        CHL_HT_ITERATOR *pItr, 
        PCVOID pvKey, 
        PINT pkeysize,
        PVOID pvVal, 
        PINT pvalsize,
        BOOL fGetPointerOnly);

    void (*Dump)(struct _hashtable* phtable);

}CHL_HTABLE, *PCHL_HTABLE;

// -------------------------------------------
// Functions exported

// Creates a hashtable and returns a pointer which can be used for later operations
// on the table.
// Params:
//      pHTableOut: Address of pointer where to copy the pointer to the hashtable
//      nEstEntries: Estimated number of entries that would be in the table at any given time.
//                   This is used to determine the initial size of the hashtable.
//      keyType: Type of variable that is used as key - a string or a number
//      valType: Type of value that is stored - number, string or void(can be anything)
//      fValInHeapMem: Set this to true if the value(type is CHL_VT_POINTER) is allocated memory on the heap.
//                     This indicates the hash table to free it when a table entry is removed.
// 
DllExpImp HRESULT CHL_DsCreateHT(
    _Inout_ CHL_HTABLE **pHTableOut, 
    _In_ int nEstEntries, 
    _In_ CHL_KEYTYPE keyType, 
    _In_ CHL_VALTYPE valType, 
    _In_opt_ BOOL fValInHeapMem);

DllExpImp HRESULT CHL_DsDestroyHT(_In_ CHL_HTABLE *phtable);

// Inserts a key,value pair into the hash table. If the key already exists, then the value is over-written
// with the new value. If both the key and value already exist, then nothing is changed in the hash table.
// Params:
//      phtable: Pointer to the hashtable object returned by CHL_DsCreateHT function.
//      pvkey: Pointer to the key.
//      iKeySize: For strings(ANSI and UNICODE), this the number of characters including terminating NULL.
//               For other types, it is the size in bytes.
//      pvVal: Value to be stored. Please see documentation for details.
//      iValSize: For strings(ANSI and UNICODE), number of characters including terminating NULL.
//                For other types, it is the size in bytes.
//
DllExpImp HRESULT CHL_DsInsertHT(
    _In_ CHL_HTABLE *phtable, 
    _In_ PCVOID pvkey, 
    _In_ int iKeySize, 
    _In_ PCVOID pvVal, 
    _In_ int iValSize);

DllExpImp HRESULT CHL_DsFindHT(
    _In_ CHL_HTABLE *phtable, 
    _In_ PCVOID pvkey, 
    _In_ int iKeySize, 
    _Inout_opt_ PVOID pvVal, 
    _Inout_opt_ PINT pvalsize,
    _In_opt_ BOOL fGetPointerOnly);

DllExpImp HRESULT CHL_DsRemoveHT(_In_ CHL_HTABLE *phtable, _In_ PCVOID pvkey, _In_ int iKeySize);

DllExpImp HRESULT CHL_DsInitIteratorHT(_In_ CHL_HT_ITERATOR *pItr);
DllExpImp HRESULT CHL_DsGetNextHT(
    _In_ CHL_HTABLE *phtable, 
    _In_ CHL_HT_ITERATOR *pItr, 
    _Inout_opt_ PCVOID pvKey, 
    _Inout_opt_ PINT pkeysize,
    _Inout_opt_ PVOID pvVal, 
    _Inout_opt_ PINT pvalsize,
    _In_opt_ BOOL fGetPointerOnly);

DllExpImp int CHL_DsGetNearestSizeIndexHT(_In_ int maxNumberOfEntries);
DllExpImp void CHL_DsDumpHT(_In_ CHL_HTABLE *phtable);

// Exposing for unit testing
DllExpImp DWORD _GetKeyHash(_In_ PVOID pvKey, _In_ CHL_KEYTYPE keyType, _In_ int iKeySize, _In_ int iTableNodes);

#ifdef __cplusplus
}
#endif

#endif // _HASHTABLE_H
