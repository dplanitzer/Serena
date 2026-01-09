//
//  ext/queue.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <ext/queue.h>


//
// A de-queue.
//

void deque_add_first(deque_t* _Nonnull dq, deque_node_t* _Nonnull node)
{
    node->prev = NULL;
    node->next = dq->first;
    
    if (dq->first) {
        dq->first->prev = node;
    }
    
    dq->first = node;
    if (dq->last == NULL) {
        dq->last = node;
    }
}

void deque_add_last(deque_t* _Nonnull dq, deque_node_t* _Nonnull node)
{
    node->prev = dq->last;
    node->next = NULL;
    
    if (dq->last) {
        dq->last->next = node;
    }
    
    dq->last = node;
    if (dq->first == NULL) {
        dq->first = node;
    }
}

// Inserts the node 'pNode' after 'pAfterNode'. The node 'pNode' is added as the
// first node in the list if 'pAfterNode' is NULL.
void deque_insert(deque_t* _Nonnull dq, deque_node_t* _Nonnull node, deque_node_t* _Nullable after)
{
    if (after) {
        node->prev = after;
        node->next = after->next;
    
        if (after->next) {
            after->next->prev = node;
        }
        after->next = node;
    
        if (dq->last == after) {
            dq->last = node;
        }
    } else {
        deque_add_first(dq, node);
    }
}

void deque_remove(deque_t* _Nonnull dq, deque_node_t* _Nonnull node)
{
    if (dq->first != node || dq->last != node) {
        if (node->next) {
            node->next->prev = node->prev;
        }
        if (node->prev) {
            node->prev->next = node->next;
        }
        
        if (dq->first == node) {
            dq->first = node->next;
        }
        if (dq->last == node) {
            dq->last = node->prev;
        }
    } else {
        dq->first = NULL;
        dq->last = NULL;
    }
    
    node->prev = NULL;
    node->next = NULL;
}

deque_node_t* _Nullable deque_remove_first(deque_t* _Nonnull dq)
{
    deque_node_t* first = dq->first;

    if (first != NULL) {
        if (first != dq->last) {
            deque_node_t* second = first->next;

            second->prev = NULL;
            dq->first = second;
            
            first->prev = NULL;
            first->next = NULL;
        } else {
            dq->first = NULL;
            dq->last = NULL;
        }
    }

    return first;
}


//
// A singly linked list.
//

// Inserts the node 'pNode' after 'pAfterNode'. The node 'pNode' is added as the
// first node in the list if 'pAfterNode' is NULL.
void SList_InsertAfter(SList* _Nonnull pList, SListNode* _Nonnull pNode, SListNode* _Nullable pAfterNode)
{
    if (pAfterNode) {
        pNode->next = pAfterNode->next;
        pAfterNode->next = pNode;
    
        if (pList->last == pAfterNode) {
            pList->last = pNode;
        }
    } else {
        SList_InsertBeforeFirst(pList, pNode);
    }
}

SListNode* _Nullable SList_RemoveFirst(SList* _Nonnull pList)
{
    SListNode* np = pList->first;

    if (np) {
        pList->first = np->next;
        if (pList->last == np) {
            pList->last = NULL;
        }
        np->next = NULL;
    }
    return np;
}

// Removes 'pNodeToRemove' from 'pList'. 'pPrevNode' must point to the predecessor
// node of 'pNodeToRemove'. It may only be NULL if 'pNodeToRemove' is the first
// node in the list or 'pNodeToRemove' is the last remaining node in the list.
void SList_Remove(SList* _Nonnull pList, SListNode* _Nullable pPrevNode, SListNode* _Nonnull pNodeToRemove)
{
    if (pNodeToRemove == pList->first && pNodeToRemove == pList->last) {
        pList->first = NULL;
        pList->last = NULL;
    }
    else if (pNodeToRemove == pList->first) {
        pList->first = pNodeToRemove->next;
    }
    else if (pNodeToRemove == pList->last) {
        pList->last = pPrevNode;
        if (pPrevNode) {
            pPrevNode->next = NULL;
        }
    }
    else {
        pPrevNode->next = pNodeToRemove->next;
    }
}
