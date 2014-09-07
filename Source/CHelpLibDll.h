
// CHelpLib.h
// Exported symbols from CHelpLib DLL
// Shishir K Prasad (http://www.shishirprasad.net)
// History
//      06/23/13 Initial version
//      03/13/14 Function name modifications. Naming convention.

#ifndef _CHELPLIBDLL_H
#define _CHELPLIBDLL_H

#ifdef __cplusplus
extern "C" {  
#endif

#ifdef CHELPLIB_EXPORTS
#define DllExpImp __declspec( dllexport )
#else
#define DllExpImp __declspec( dllimport )
#endif // CHELPLIB_EXPORTS

// Defines

// Custom error codes
#define CHLE_MEM_ENOMEM     17000
#define CHLE_MEM_GEN        17001
#define CHLE_MUTEX_TIMEOUT  17010
#define CHLE_EMPTY_FILE     17011

#define CHLE_LLIST_EINVAL   17100
#define CHLE_LLIST_VALTYPE  17101

#define CHLE_PROC_DOSHEADER     17150
#define CHLE_PROC_TEXTSECHDR    17151
#define CHLE_PROC_NOEXEC        17152

// hashtable key types
#define HT_KEY_STR      10
#define HT_KEY_DWORD    11

// hashtable value types
#define HT_VAL_DWORD    12
#define HT_VAL_PTR      13
#define HT_VAL_STR      14

// LinkedList value types
#define LL_VAL_BYTE     20
#define LL_VAL_UINT     21
#define LL_VAL_DWORD    22
#define LL_VAL_LONGLONG 23
#define LL_VAL_PTR      24

// Key Types
#define CHL_KT_STRING   10
#define CHL_KT_INT32    11

// Value Types
#define CHL_VT_STRING   20
#define CHL_VT_INT32    21
#define CHL_VT_PVOID    22

// Key type and Value type typedefs
typedef int CHL_KEYTYPE;
typedef int CHL_VALTYPE;

typedef int HT_KEYTYPE;
typedef int HT_VALTYPE;
typedef int LL_VALTYPE;

// Structures
union _key {
    BYTE *skey;
    DWORD dwkey;
};

union _val {
    DWORD dwVal;
    void *pval;
    BYTE *pbVal;
};

#pragma region Structs_Hashtable

// hashtable node
typedef struct _hashTableNode {
    //BOOL fOccupied;     // FALSE = not occupied
    union _key key;
    union _val val;
    int keysize;
    int valsize;
    struct _hashTableNode *pnext;
}HT_NODE;

// hashtable itself
typedef struct _hashtable {
    HT_KEYTYPE htKeyType;
    HT_VALTYPE htValType;
    BOOL fValIsInHeap;
    HT_NODE **phtNodes;
    int nTableSize;
    HANDLE hMuAccess;
}CHL_HTABLE;

// Structure that defines the iterator for the hashtable
// Callers can use this to iterate through the hashtable
// and get all (key,value) pairs one-by-one
typedef struct _hashtableIterator {
    int opType;
    int nCurIndex;              // current position in the main bucket
    HT_NODE *phtCurNodeInList;  // current position in the sibling list
}CHL_HT_ITERATOR;

#pragma endregion Structs_Hashtable

#pragma region Structs_LinkedList

// ******** Linked List ********
union _nodeval {
    BYTE bval;
    UINT uval;
    DWORD dwval;
    LONGLONG longlongval;
    void *pval;
};

typedef struct _LlNode {
    union _nodeval nodeval;
    DWORD dwValSize;
    struct _LlNode *pleft;
    struct _LlNode *pright;
}LLNODE, *PLLNODE;

typedef struct _LinkedList {
    int nCurNodes;
    int nMaxNodes;
    LL_VALTYPE valType;
    PLLNODE pHead;
    PLLNODE pTail;
    HANDLE hMuAccess;
}CHL_LLIST, *PCHL_LLIST;

#pragma endregion Structs_LinkedList

// A Queue object structure
typedef struct _Queue
{
    int nCapacity;  // unused currently
    int nCurItems;
    PCHL_LLIST pList;

    // Access Methods
    HRESULT (*Destroy)(struct _Queue* pThis);
    HRESULT (*Insert)(struct _Queue* pThis, PVOID pVal, int nValSize);
    HRESULT (*Delete)(struct _Queue* pThis, PVOID* ppVal);
    HRESULT (*Peek)(struct _Queue* pThis, PVOID* ppVal);
    HRESULT (*Find)(struct _Queue* pThis, PVOID pValToFind, BOOL (*pfnComparer)(void*, void*));
}CHL_QUEUE, *PCHL_QUEUE;


// ** Functions exported **

// MemFunctions
DllExpImp BOOL fChlMmAlloc(__out void **pvAddr, __in size_t uSizeBytes, OPTIONAL DWORD *pdwError);
DllExpImp void vChlMmFree(__in void **pvToFree);

// IOFunctions
DllExpImp BOOL fChlIoReadLineFromStdin(__in DWORD dwBufSize, __out WCHAR *psBuffer);
DllExpImp BOOL fChlIoCreateFileWithSize(__in PWCHAR pszFilepath, __in int iSizeBytes, __out PHANDLE phFile);

// StringFunctions
DllExpImp WCHAR* pszChlSzGetFilenameFromPath(WCHAR *pwsFilepath, int numCharsInput);

// DataStructure Functions
// Hastable functions

// Creates a hashtable and returns a pointer which can be used for later operations
// on the table.
// params:
//      pHTableOut: Address of pointer where to copy the pointer to the hashtable
//      nEstEntries: Estimated number of entries that would be in the table at any given time.
//                   This is used to determine the initial size of the hashtable.
//      keyType: Type of variable that is used as key - a string or a number
//      valType: Type of value that is stored - number, string or void(can be anything)
//      fValInHeapMem: Set this to true if the value(void type) is allocated memory on the heap
//                     so that it is freed whenever an table entry is removed or when table is destroyed.
// 
DllExpImp BOOL fChlDsCreateHT(CHL_HTABLE **pHTableOut, int nEstEntries, int keyType, int valType, BOOL fValInHeapMem);
DllExpImp BOOL fChlDsDestroyHT(CHL_HTABLE *phtable);

DllExpImp BOOL fChlDsInsertHT (CHL_HTABLE *phtable, void *pvkey, int keySize, void *pval, int valSize);
DllExpImp BOOL fChlDsFindHT   (CHL_HTABLE *phtable, void *pvkey, int keySize, __in_opt void *pval, __in_opt int *pvalsize);
DllExpImp BOOL fChlDsRemoveHT (CHL_HTABLE *phtable, void *pvkey, int keySize);

DllExpImp BOOL fChlDsInitIteratorHT(CHL_HT_ITERATOR *pItr);
DllExpImp BOOL fChlDsGetNextHT(CHL_HTABLE *phtable, CHL_HT_ITERATOR *pItr, 
                            __out void *pkey, __out int *pkeysize,
                            __out void *pval, __out int *pvalsize);

DllExpImp int  iChlDsGetNearestTableSizeIndex(int maxNumberOfEntries);
DllExpImp void vChlDsDumpHT(CHL_HTABLE *phtable);

// Linked List Functions
DllExpImp BOOL fChlDsCreateLL(__out PCHL_LLIST *ppLList, LL_VALTYPE valType, OPTIONAL int nEstEntries);
DllExpImp BOOL fChlDsInsertLL(PCHL_LLIST pLList, void *pval, int valsize);
DllExpImp BOOL fChlDsRemoveLL(PCHL_LLIST pLList, void *pvValToFind, BOOL fStopOnFirstFind, BOOL (*pfnComparer)(void*, void*), __out OPTIONAL void **ppval);
DllExpImp BOOL fChlDsRemoveAtLL(PCHL_LLIST pLList, int iIndexToRemove, __out OPTIONAL void **ppval);
DllExpImp BOOL fChlDsPeekAtLL(PCHL_LLIST pLList, int iIndexToPeek, __out void **ppval);
DllExpImp BOOL fChlDsFindLL(PCHL_LLIST pLList, __in void *pvValToFind, BOOL (*pfnComparer)(void*, void*), __out OPTIONAL void **ppval);
DllExpImp BOOL fChlDsDestroyLL(PCHL_LLIST pLList);

// Queue Functions
DllExpImp HRESULT CHL_QueueCreate(_Out_ PCHL_QUEUE *ppQueueObj, _In_ CHL_VALTYPE valType, _In_opt_ int nEstimatedItems);
DllExpImp HRESULT CHL_QueueDestroy(_In_ PCHL_QUEUE pQueueObj);
DllExpImp HRESULT CHL_QueueInsert(_In_ PCHL_QUEUE pQueueObj, _In_ PVOID pvValue, _In_ int nValSize);
DllExpImp HRESULT CHL_QueueDelete(_In_ PCHL_QUEUE pQueueObj, _Out_ PVOID *ppvValue);
DllExpImp HRESULT CHL_QueuePeek(_In_ PCHL_QUEUE pQueueObj, _Out_ PVOID *ppvValue);
DllExpImp HRESULT CHL_QueueFind(_In_ PCHL_QUEUE pQueueObj, _In_ PVOID pvValue, _In_opt_ BOOL (*pfnComparer)(void*, void*));

// Gui Functions
DllExpImp BOOL fChlGuiCenterWindow(HWND hWnd);

// Given the window handle and the number of characters, returns the 
// width and height in pixels that will be occupied by a string of that
// consisting of those number of characters
DllExpImp BOOL fChlGuiGetTextArea(HWND hWindow, int nCharsInText, __out int *pnPixelsWidth, __out int *pnPixelsHeight);
DllExpImp BOOL fChlGuiInitListViewColumns(HWND hList, WCHAR *apszColumNames[], int nColumns, OPTIONAL int *paiColumnSizePercent);
DllExpImp BOOL fChlGuiAddListViewRow(HWND hList, WCHAR *apszItemText[], int nItems, __in_opt LPARAM lParam);

// Process Functions
DllExpImp BOOL fChlPsGetProcNameFromID(DWORD pid, WCHAR *pwsProcName, DWORD dwBufSize);
DllExpImp BOOL fChlPsGetNtHeaders(HANDLE hMapView, __out PIMAGE_NT_HEADERS *ppstNtHeaders);
DllExpImp BOOL fChlPsGetPtrToCode(
                                DWORD dwFileBase, 
                                PIMAGE_NT_HEADERS pNTHeaders, 
                                __out PDWORD pdwCodePtr, 
                                __out PDWORD pdwSizeOfData,
                                __out PDWORD pdwCodeSecVirtAddr);
DllExpImp BOOL fChlPsGetEnclosingSectionHeader(DWORD rva, PIMAGE_NT_HEADERS pNTHeader, __out PIMAGE_SECTION_HEADER *ppstSecHeader);

// General functions
DllExpImp BOOL fChlGnIsOverflowINT(int a, int b);
DllExpImp BOOL fChlGnIsOverflowUINT(unsigned int a, unsigned int b);
DllExpImp BOOL fChlGnOwnMutex(HANDLE hMutex);
DllExpImp BOOL fChlGnCreateMemMapOfFile(HANDLE hFile, DWORD dwReqProtection, __out PHANDLE phMapObj, __out PHANDLE phMapView);

#ifdef __cplusplus
}
#endif

#endif // _CHELPLIBDLL_H
