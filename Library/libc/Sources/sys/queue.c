//
//  sys/queue.c
//  libc
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <sys/queue.h>


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
    SListNode* pFirstNode = pList->first;

    if (pFirstNode != NULL) {
        if (pFirstNode != pList->last) {
            SListNode* pNewFirstNode = pFirstNode->next;

            pList->first = pNewFirstNode;
            pFirstNode->next = NULL;
        } else {
            pList->first = NULL;
            pList->last = NULL;
        }
    }

    return pFirstNode;
}
