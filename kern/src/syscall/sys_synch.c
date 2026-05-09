//
//  sys_synch.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/atomic.h>
#include <ext/hash.h>
#include <ext/queue.h>
#include <hal/sched.h>
#include <kern/kalloc.h>
#include <kpi/signal.h>
#include <kpi/synch.h>
#include <sched/mtx.h>
#include <sched/waitqueue.h>


// TODO
//
// *) reduce contention on the g_ww_mtx: do this by introducing a partitioning of
//    the g_ww_table. Have a g_ww_part_table which is sized and initialized at
//    boot time and thereafter treated as a read-only data structure so that we
//    don't need to protect it with a separate lock. The table then holds table
//    entries where each table is a g_ww_table style hash table with its own
//    separate mutex. Then to do the look up of a ww_hdr_t, calculate its hash
//    value (sanse the table size mod value), look up the g_ww_table for the
//    partition, lock it using its mutex and then find the ww_hdr_t in there.
//
// *) ww_hdr_t use_count can overflow in theory. Maybe return EAGAIN in this case
//    and require that user space does something like vcpu_yield() before it tries
//    again. Or maybe do it in the kernel although I'd prefer user space to do it
//    to keep the kernel side simpler.
//
// *) replace dequeue_t with a single linked list if reasonable.


#define WW_HASH_CHAIN_COUNT 8
#define WW_HASH_CHAIN_MASK  (WW_HASH_CHAIN_COUNT - 1)

#define WW_CACHE_CAPACITY   16

struct ww_hdr {
    deque_node_t        qe;
    struct waitqueue    wq;
    void*               key;
    int                 use_count;
};
typedef struct ww_hdr* ww_hdr_t;

static deque_t/*<struct ww_hdr>*/   g_ww_table[WW_HASH_CHAIN_COUNT];    // key (addr) -> struct ww_hdr
static deque_t/*<struct ww_hdr>*/   g_ww_cache;
static size_t                       g_ww_cache_size;
static mtx_t                        g_ww_mtx;


static ww_hdr_t _Nullable _find_ww_locked(void* key)
{
    deque_for_each(&g_ww_table[hash_ptr(key) & WW_HASH_CHAIN_MASK], struct ww_hdr, it,
        if (it->key == key) {
            return it;
        }
    )

    return NULL;
}

static ww_hdr_t _Nullable _acquire_ww_for_addr(void* key, bool bAllocIfNeeded)
{
    mtx_lock(&g_ww_mtx);
    ww_hdr_t wwp = _find_ww_locked(key);
    
    if (wwp == NULL && bAllocIfNeeded) {
        if (g_ww_cache_size > 0) {
            wwp = (ww_hdr_t)deque_remove_first(&g_ww_cache);
            g_ww_cache_size--;
        }
        else {
            if (kalloc(sizeof(struct ww_hdr), (void**)&wwp) != EOK) {
                mtx_unlock(&g_ww_mtx);
                return NULL;
            }
        }


        wwp->qe = DEQUE_NODE_INIT;
        wwp->wq = WAITQUEUE_INIT;
        wwp->key = key;
        wwp->use_count = 0;

        deque_add_last(&g_ww_table[hash_ptr(key) & WW_HASH_CHAIN_MASK], &wwp->qe);
    }


    if (wwp) {
        wwp->use_count++;
    }
    mtx_unlock(&g_ww_mtx);

    return wwp;
}

static void _relinquish_ww(ww_hdr_t _Nonnull wwp)
{
    mtx_lock(&g_ww_mtx);

    wwp->use_count--;
    if (wwp->use_count == 0) {
        // remove the ww from the hash table
        deque_remove(&g_ww_table[hash_ptr(wwp->key) & WW_HASH_CHAIN_MASK], &wwp->qe);


        // put it on the cache queue if there's still space left; otherwise free it
        // for good
        if (g_ww_cache_size < WW_CACHE_CAPACITY) {
            deque_add_first(&g_ww_cache, &wwp->qe);
            g_ww_cache_size++;
        }
        else {
            wq_deinit(&wwp->wq);
            kfree(wwp);
        }
    }

    mtx_unlock(&g_ww_mtx);
}


SYSCALL_2(ww_wait, volatile atomic_int* _Nonnull addr, int expected)
{
    decl_try_err();
    ww_hdr_t wwp = _acquire_ww_for_addr(pa->addr, true);
    if (wwp == NULL) {
        return ENOMEM;
    }
    

    const int sps = preempt_disable();
    for (;;) {
        if ((vp->pending_sigs & sig_bit(SIG_TERMINATE)) != 0) {
            err = EINTR;
            break;
        }
        if (atomic_int_load(pa->addr) != pa->expected) {
            break;
        }

        wq_wait_np(&wwp->wq);
    }
    preempt_restore(sps);
    

    _relinquish_ww(wwp);

    return err;
}

SYSCALL_4(ww_timedwait, volatile atomic_int* _Nonnull addr, int expected, int flags, const nanotime_t* _Nonnull wtp)
{
    decl_try_err();
    ww_hdr_t wwp = _acquire_ww_for_addr(pa->addr, true);
    if (wwp == NULL) {
        return ENOMEM;
    }
    

    const int sps = preempt_disable();
    for (;;) {
        if ((vp->pending_sigs & sig_bit(SIG_TERMINATE)) != 0) {
            err = EINTR;
            break;
        }
        if (atomic_int_load(pa->addr) != pa->expected) {
            break;
        }

        if (wq_timedwait_np(&wwp->wq, pa->flags, pa->wtp)) {
            err = ETIMEDOUT;
        }
    }
    preempt_restore(sps);
    

    _relinquish_ww(wwp);

    return err;
}

SYSCALL_2(ww_wakeup, volatile atomic_int* _Nonnull addr, int flags)
{
    if ((pa->flags & ~_WAKEUP_UMASK) != 0) {
        return EINVAL;
    }

    ww_hdr_t wwp = _acquire_ww_for_addr(pa->addr, false);
    if (wwp) {
        const int sps = preempt_disable();
        wq_wakeup_many_np(&wwp->wq, pa->flags, 0);
        preempt_restore(sps);

        _relinquish_ww(wwp);
    }

    return EOK;
}
