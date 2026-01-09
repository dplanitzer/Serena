//
//  ext/queue.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_QUEUE_H
#define _EXT_QUEUE_H 1

#include <_cmndef.h>
#include <stddef.h>

__CPP_BEGIN

//
// A de-queue.
//

typedef struct deque_node {
    struct deque_node* _Nullable  next;
    struct deque_node* _Nullable  prev;
} deque_node_t;

typedef struct deque_t {
    deque_node_t* _Nullable first;
    deque_node_t* _Nullable last;
} deque_t;


#define DEQUE_NODE_INIT (deque_node_t){NULL, NULL}
#define DEQUE_INIT      (deque_t){NULL, NULL}

// Adds 'node' before the first node in the dequeue.
extern void deque_add_first(deque_t* _Nonnull dq, deque_node_t* _Nonnull node);

// Adds 'node' after the last node in the dequeue.
extern void deque_add_last(deque_t* _Nonnull dq, deque_node_t* _Nonnull node);
extern void deque_insert(deque_t* _Nonnull dq, deque_node_t* _Nonnull node, deque_node_t* _Nullable after);

extern void deque_remove(deque_t* _Nonnull dq, deque_node_t* _Nonnull node);
extern deque_node_t* _Nullable deque_remove_first(deque_t* _Nonnull dq);

#define deque_empty(__dq) \
((__dq)->first == NULL)

// Iterates all elements of the given deque. Guarantees that the closure may call
// free on 'pCurNode' without ill effect. The iteration will continue until the
// end of the deque is reached or 'closure' executes a break statement. 
#define deque_for_each(dq, NodeType, closure) \
{ \
    NodeType* pCurNode = (NodeType*)(dq)->first; \
    while (pCurNode) { \
        NodeType* pNextNode = (NodeType*)((deque_node_t*)pCurNode)->next; \
        { closure } \
        pCurNode = pNextNode; \
    } \
}

// Iterates all elements of the given deque in reverse order. Guarantees that the
// closure may call free on 'pCurNode' without ill effect. The iteration will
// continue until the end of the deque is reached or 'closure' executes a break
// statement. 
#define deque_for_each_reversed(dq, NodeType, closure) \
{ \
    NodeType* pCurNode = (NodeType*)(dq)->last; \
    while (pCurNode) { \
        NodeType* pPrevNode = (NodeType*)((deque_node_t*)pCurNode)->prev; \
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

__CPP_END

#endif /* _EXT_QUEUE_H */
