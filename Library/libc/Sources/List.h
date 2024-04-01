//
//  List.h
//  libc
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _LIST_H
#define _LIST_H 1

#include <__stddef.h>

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


static inline bool SList_IsEmpty(SList* _Nonnull pList) {
    return pList->first == NULL;
}

#define SList_ForEach(pList, NodeType, closure) \
{\
    NodeType* pCurNode = (NodeType*)(pList)->first; \
    while (pCurNode) { \
        NodeType* pNextNode = (NodeType*)((SListNode*)pCurNode)->next; \
        { closure } \
        pCurNode = pNextNode; \
    } \
}

#endif /* _LIST_H */
