//
//  ext/queue.c
//  libc, libsc
//
//  Created by Dietmar Planitzer on 2/17/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <ext/queue.h>


void queue_add_first(queue_t* _Nonnull q, queue_node_t* _Nonnull node)
{
    _queue_add_first(q, node);
}

void queue_add_last(queue_t* _Nonnull q, queue_node_t* _Nonnull node)
{
    _queue_add_last(q, node);
}

void queue_insert(queue_t* _Nonnull q, queue_node_t* _Nonnull node, queue_node_t* _Nullable after)
{
    if (after) {
        node->next = after->next;
        after->next = node;
    
        if (q->last == after) {
            q->last = node;
        }
    } else {
        queue_add_first(q, node);
    }
}

queue_node_t* _Nullable queue_remove_first(queue_t* _Nonnull q)
{
    queue_node_t* np = q->first;

    if (np) {
        q->first = np->next;
        if (q->last == np) {
            q->last = NULL;
        }
        np->next = NULL;
    }
    return np;
}

void queue_remove(queue_t* _Nonnull q, queue_node_t* _Nullable prev, queue_node_t* _Nonnull node)
{
    if (node == q->first && node == q->last) {
        q->first = NULL;
        q->last = NULL;
    }
    else if (node == q->first) {
        q->first = node->next;
    }
    else if (node == q->last) {
        q->last = prev;
        if (prev) {
            prev->next = NULL;
        }
    }
    else {
        prev->next = node->next;
    }
}
