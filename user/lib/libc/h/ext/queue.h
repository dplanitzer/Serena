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
// A queue.
//

typedef struct queue_node {
    struct queue_node* _Nullable    next;
} queue_node_t;

typedef struct queue_t {
    queue_node_t* _Nullable first;
    queue_node_t* _Nullable last;
} queue_t;


#define QUEUE_NODE_INIT (queue_node_t){NULL}
#define QUEUE_INIT      (queue_t){NULL, NULL}

#define _queue_add_first(q, node) \
(node)->next = (q)->first; \
(q)->first = (node); \
if ((q)->last == NULL) { \
    (q)->last = node; \
}

#define _queue_add_last(q, node) \
(node)->next = NULL; \
if ((q)->last) { \
    (q)->last->next = node; \
} \
(q)->last = node; \
if ((q)->first == NULL) { \
    (q)->first = node; \
}

#define _queue_remove_first(q) \
if ((q)->first) { \
    queue_node_t* __fnp = (q)->first; \
    if ((q)->last == __fnp) { \
        (q)->last = NULL; \
    } \
    (q)->first = __fnp->next; \
    __fnp->next = NULL; \
}


extern void queue_add_first(queue_t* _Nonnull q, queue_node_t* _Nonnull node);
extern void queue_add_last(queue_t* _Nonnull q, queue_node_t* _Nonnull node);
extern void queue_insert(queue_t* _Nonnull q, queue_node_t* _Nonnull node, queue_node_t* _Nullable after);

queue_node_t* _Nullable queue_remove_first(queue_t* _Nonnull q);

// Removes 'node' from 'q'. 'prev' must point to the predecessor node of 'node'.
// It may only be NULL if 'node' is the first node in the queue or 'node' is the
// last remaining node in the queue.
void queue_remove(queue_t* _Nonnull q, queue_node_t* _Nullable prev, queue_node_t* _Nonnull node);


#define queue_empty(__q) \
((__q)->first == NULL)

#define queue_node_as(__qe_ptr, __qe_field_name, __type) \
(struct __type*) (((char*)__qe_ptr) - offsetof(struct __type, __qe_field_name))

// Iterates all elements of the given queue. Guarantees that the closure may call
// free on 'it' without ill effect. The iteration will continue until the end of
// the queue is reached or 'closure' executes a break statement. 
#define queue_for_each(q, NodeType, it, closure) \
{\
    NodeType* (it) = (NodeType*)(q)->first; \
    while (it) { \
        NodeType* __pn = (NodeType*)((queue_node_t*)(it))->next; \
        { closure } \
        (it) = __pn; \
    } \
}


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
extern deque_node_t* _Nullable deque_remove_last(deque_t* _Nonnull dq);


#define deque_empty(__dq) \
((__dq)->first == NULL)

#define deque_node_as(__qe_ptr, __qe_field_name, __type) \
(struct __type*) (((char*)__qe_ptr) - offsetof(struct __type, __qe_field_name))


// Iterates all elements of the given deque. Guarantees that the closure may call
// free on 'it' without ill effect. The iteration will continue until the end of
// the deque is reached or 'closure' executes a break statement. 
#define deque_for_each(dq, NodeType, it, closure) \
{ \
    NodeType* (it) = (NodeType*)(dq)->first; \
    while (it) { \
        NodeType* __np = (NodeType*)((deque_node_t*)(it))->next; \
        { closure } \
        (it) = __np; \
    } \
}

// Iterates all elements of the given deque in reverse order. Guarantees that the
// closure may call free on 'it' without ill effect. The iteration will continue
// until the end of the deque is reached or 'closure' executes a break statement. 
#define deque_for_each_reversed(dq, NodeType, it, closure) \
{ \
    NodeType* (it) = (NodeType*)(dq)->last; \
    while (it) { \
        NodeType* __pp = (NodeType*)((deque_node_t*)(it))->prev; \
        { closure } \
        (it) = __pp; \
    } \
}

__CPP_END

#endif /* _EXT_QUEUE_H */
