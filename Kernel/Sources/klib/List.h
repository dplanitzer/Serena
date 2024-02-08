//
//  List.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef List_h
#define List_h

#include <klib/Types.h>


//
// A doubly linked list.
//

typedef struct _ListNode {
    struct _ListNode* _Nullable next;
    struct _ListNode* _Nullable prev;
} ListNode;

typedef struct _List {
    ListNode* _Nullable first;
    ListNode* _Nullable last;
} List;


static inline void List_Init(List* _Nonnull pList) {
    pList->first = NULL;
    pList->last = NULL;
}

static inline void List_Deinit(List* _Nonnull pList) {
    pList->first = NULL;
    pList->last = NULL;
}

static inline void ListNode_Init(ListNode* _Nonnull pNode) {
    pNode->prev = NULL;
    pNode->next = NULL;
}

static inline void ListNode_Deinit(ListNode* _Nonnull pNode) {
    pNode->prev = NULL;
    pNode->next = NULL;
}

extern void List_InsertBeforeFirst(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern void List_InsertAfterLast(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern void List_InsertAfter(List* _Nonnull pList, ListNode* _Nonnull pNode, ListNode* _Nullable pAfterNode);

extern void List_Remove(List* _Nonnull pList, ListNode* _Nonnull pNode);
extern ListNode* _Nullable List_RemoveFirst(List* _Nonnull pList);

extern void List_Split(List* _Nonnull pList, ListNode* _Nullable pFirstNodeOfTail, List* _Nonnull pHeadList, List* _Nonnull pTailList);

static inline bool List_IsEmpty(List* _Nonnull pList) {
    return pList->first == NULL;
}

#define List_ForEach(pList, NodeType, closure) \
for(NodeType* pCurNode = (NodeType*)(pList)->first; pCurNode != NULL; pCurNode = (NodeType*)((ListNode*)pCurNode)->next) { closure }

#define List_ForEachReversed(pList, NodeType, closure) \
for(NodeType* pCurNode = (NodeType*)(pList)->last; pCurNode != NULL; pCurNode = (NodeType*)((ListNode*)pCurNode)->prev) { closure }


//
// A singly linked list.
//

typedef struct _SListNode {
    struct _SListNode* _Nullable    next;
} SListNode;

typedef struct _SList {
    SListNode* _Nullable    first;
    SListNode* _Nullable    last;
} SList;


static inline void SList_Init(SList* _Nonnull pList) {
    pList->first = NULL;
    pList->last = NULL;
}

static inline void SList_Deinit(SList* _Nonnull pList) {
    pList->first = NULL;
    pList->last = NULL;
}

static inline void SListNode_Init(SListNode* _Nonnull pNode) {
    pNode->next = NULL;
}

static inline void SListNode_Deinit(SListNode* _Nonnull pNode) {
    pNode->next = NULL;
}

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


static inline bool SList_IsEmpty(SList* _Nonnull pList) {
    return pList->first == NULL;
}

#define SList_ForEach(pList, NodeType, closure) \
for(NodeType* pCurNode = (NodeType*)(pList)->first; pCurNode != NULL; pCurNode = (NodeType*)((SListNode*)pCurNode)->next) { closure }

#endif /* List_h */
