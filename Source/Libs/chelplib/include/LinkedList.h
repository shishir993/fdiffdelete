
// LinkedList.h
// Contains functions that implement a linked list data structure
// Shishir Bhat (http://www.shishirbhat.com)
// History
//      04/05/14 Initial version
//      09/09/14 Refactor to store defs in individual headers.
//      09/12/14 Naming convention modifications
//

#ifndef _LINKEDLIST_H
#define _LINKEDLIST_H

#ifdef __cplusplus
extern "C" {  
#endif

#include "Defines.h"

// Custom error codes
#define CHLE_LLIST_EINVAL   17100
#define CHLE_LLIST_VALTYPE  17101

typedef struct _LlNode {
    CHL_VAL chlVal;
    struct _LlNode *pleft;
    struct _LlNode *pright;
}LLNODE, *PLLNODE;

typedef struct _LinkedList {
    int nCurNodes;
    int nMaxNodes;
    CHL_VALTYPE valType;
    PLLNODE pHead;
    PLLNODE pTail;
	CRITICAL_SECTION csLock;

    // Access methods
    HRESULT (*Insert)(struct _LinkedList* pLList, PCVOID pvVal, int iValSize);

    HRESULT (*Remove)(
        struct _LinkedList* pLList, 
        PCVOID pvValToFind, 
        BOOL fStopOnFirstFind, 
        BOOL (*pfnComparer)(PCVOID, PCVOID));

    HRESULT (*RemoveAt)(
        struct _LinkedList* pLList, 
        int iIndexToRemove, 
        PVOID pvValOut,
        PINT piValBufSize,
        BOOL fGetPointerOnly);

    HRESULT (*Peek)(
        struct _LinkedList* pLList, 
        int iIndexToPeek, 
        PVOID pvValOut,
        PINT piValBufSize,
        BOOL fGetPointerOnly);

    HRESULT (*Find)(
        struct _LinkedList* pLList, 
        PCVOID pvValToFind, 
        BOOL (*pfnComparer)(PCVOID, PCVOID));

    HRESULT (*Destroy)(struct _LinkedList* pLList);

}CHL_LLIST, *PCHL_LLIST;

// -------------------------------------------
// Functions exported

DllExpImp HRESULT CHL_DsCreateLL(_Out_ PCHL_LLIST *ppLList, _In_ CHL_VALTYPE valType, _In_opt_ int nEstEntries);

// CHL_DsInsertLL()
// Inserts an element into the linked list. Always inserts at the tail.
// Insertion is an O(1) operation.
//      pLList: Pointer to a linked list object that was returned by a successful call
//              to CHL_DsCreateLL.
//      pvVal: The value or pointer to the value to be stored. All intrinsic are passed by 
//              value(like: CHL_DsInsertLL(pListObj, (PVOID)intValue, 0). All other complex
//              types are passed by reference(their address) and they are stored in the heap.
//      iValSize: Size in bytes of the value. Optional for intrinsic types, mandatory for others.
//
DllExpImp HRESULT CHL_DsInsertLL(_In_ PCHL_LLIST pLList, _In_ PCVOID pvVal, _In_opt_ int iValSize);

DllExpImp HRESULT CHL_DsRemoveLL(
    _In_ PCHL_LLIST pLList, 
    _In_ PCVOID pvValToFind, 
    _In_ BOOL fStopOnFirstFind, 
    _In_ BOOL (*pfnComparer)(PCVOID, PCVOID));

DllExpImp HRESULT CHL_DsRemoveAtLL(
    _In_ PCHL_LLIST pLList, 
    _In_ int iIndexToRemove, 
    _Inout_opt_ PVOID pvValOut, 
    _Inout_opt_ PINT piValBufSize,
    _In_opt_ BOOL fGetPointerOnly);

DllExpImp HRESULT CHL_DsPeekAtLL(
    _In_ PCHL_LLIST pLList, 
    _In_ int iIndexToPeek, 
    _Inout_opt_ PVOID pvValOut, 
    _Inout_opt_ PINT piValBufSize,
    _In_opt_ BOOL fGetPointerOnly);

DllExpImp HRESULT CHL_DsFindLL(
    _In_ PCHL_LLIST pLList, 
    _In_ PCVOID pvValToFind, 
    _In_ BOOL (*pfnComparer)(PCVOID, PCVOID));

DllExpImp HRESULT CHL_DsDestroyLL(_In_ PCHL_LLIST pLList);

#ifdef __cplusplus
}
#endif

#endif // _LINKEDLIST_H
