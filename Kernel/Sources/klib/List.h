//
//  List.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef List_h
#define List_h

#include <kern/types.h>
#include <kern/kernlib.h>


//
// A doubly linked list.
//

typedef struct ListNode {
    struct ListNode* _Nullable  next;
    struct ListNode* _Nullable  prev;
} ListNode;

typedef struct List {
    ListNode* _Nullable first;
    ListNode* _Nullable last;
} List;


#define LISTNODE_INIT   (ListNode){NULL, NULL}
#define LIST_INIT       (List){NULL, NULL}

extern void List_InsertBeforeFirst(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern void List_InsertAfterLast(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern void List_InsertAfter(List* _Nonnull pList, ListNode* _Nonnull pNode, ListNode* _Nullable pAfterNode);

extern void List_Remove(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern ListNode* _Nullable List_RemoveFirst(List* _Nonnull pList);

extern void List_Split(List* _Nonnull pList, ListNode* _Nullable pFirstNodeOfTail, List* _Nonnull pHeadList, List* _Nonnull pTailList);

#define List_IsEmpty(__lp) \
((__lp)->first == NULL)

// Iterates all elements of the given list. Guarantees that the closure may call
// free on 'pCurNode' without ill effect. The iteration will continue until the
// end of the list is reached or 'closure' executes a break statement. 
#define List_ForEach(pList, NodeType, closure) \
{ \
    NodeType* pCurNode = (NodeType*)(pList)->first; \
    while (pCurNode) { \
        NodeType* pNextNode = (NodeType*)((ListNode*)pCurNode)->next; \
        { closure } \
        pCurNode = pNextNode; \
    } \
}

// Iterates all elements of the given list in reverse order. Guarantees that the
// closure may call free on 'pCurNode' without ill effect. The iteration will
// continue until the end of the list is reached or 'closure' executes a break
// statement. 
#define List_ForEachReversed(pList, NodeType, closure) \
{ \
    NodeType* pCurNode = (NodeType*)(pList)->last; \
    while (pCurNode) { \
        NodeType* pPrevNode = (NodeType*)((ListNode*)pCurNode)->prev; \
        { closure } \
        pCurNode = pPrevNode; \
    } \
}


//
// A singly linked list.
//

typedef struct SListNode {
    struct SListNode* _Nullable next;
} SListNode;

typedef struct SList {
    SListNode* _Nullable    first;
    SListNode* _Nullable    last;
} SList;


#define SLISTNODE_INIT  (SListNode){NULL}
#define SLIST_INIT      (SList){NULL, NULL}


static inline void SList_InsertBeforeFirst(SList* _Nonnull pList, SListNode* _Nonnull pNode) {
    pNode->next = pList->first;

    pList->first = pNode;
    if (pList->last == NULL) {
        pList->last = pNode;
    }
}

static inline void SList_InsertAfterLast(SList* _Nonnull pList, SListNode* _Nonnull pNode) {
    pNode->next = NULL;
    
    if (pList->last) {
        pList->last->next = pNode;
    }
    
    pList->last = pNode;
    if (pList->first == NULL) {
        pList->first = pNode;
    }
}

extern void SList_InsertAfter(SList* _Nonnull pList, SListNode* _Nonnull pNode, SListNode* _Nullable pAfterNode);

SListNode* _Nullable SList_RemoveFirst(SList* _Nonnull pList);

// Removes 'pNodeToRemove' from 'pList'. 'pPrevNode' must point to the predecessor
// node of 'pNodeToRemove'. It may only be NULL if 'pNodeToRemove' is the first
// node in the list or 'pNodeToRemove' is the last remaining node in the list.
void SList_Remove(SList* _Nonnull pList, SListNode* _Nullable pPrevNode, SListNode* _Nonnull pNodeToRemove);


#define SList_IsEmpty(__lp) \
((__lp)->first == NULL)

// Iterates all elements of the given list. Guarantees that the closure may call
// free on 'pCurNode' without ill effect. The iteration will continue until the
// end of the list is reached or 'closure' executes a break statement. 
#define SList_ForEach(pList, NodeType, closure) \
{\
    NodeType* pCurNode = (NodeType*)(pList)->first; \
    while (pCurNode) { \
        NodeType* pNextNode = (NodeType*)((SListNode*)pCurNode)->next; \
        { closure } \
        pCurNode = pNextNode; \
    } \
}


#define queue_entry_as(__qe_ptr, __qe_field_name, __type) \
(struct __type*) (((char*)__qe_ptr) - offsetof(struct __type, __qe_field_name))

#endif /* List_h */
