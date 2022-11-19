//
//  List.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef List_h
#define List_h

#include "Foundation.h"


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

extern void List_Split(List* _Nonnull pList, ListNode* _Nullable pFirstNodeOfTail, List* _Nonnull pHeadList, List* _Nonnull pTailList);

static inline Bool List_IsEmpty(List* _Nonnull pList) {
    return pList->first == NULL;
}

#define List_ForEach(pList, closure) \
for(ListNode* pCurNode = pList->first; pCurNode != NULL; pCurNode = pCurNode->next) { closure; }

#define List_ForEachReversed(pList, closure) \
for(ListNode* pCurNode = pList->last; pCurNode != NULL; pCurNode = pCurNode->prev) { closure; }


#endif /* List_h */
