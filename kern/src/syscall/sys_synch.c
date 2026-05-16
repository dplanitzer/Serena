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


#define WOA_HASH_CHAIN_COUNT 8
#define WOA_HASH_CHAIN_MASK  (WOA_HASH_CHAIN_COUNT - 1)

#define WOA_CACHE_CAPACITY   16

struct woa_hdr {
    deque_node_t        qe;
    struct waitqueue    wq;
    void*               key;
    int                 use_count;
};
typedef struct woa_hdr* woa_hdr_t;

static deque_t/*<struct woa_hdr>*/  g_woa_table[WOA_HASH_CHAIN_COUNT];    // key (addr) -> struct ww_hdr
static deque_t/*<struct woa_hdr>*/  g_woa_cache;
static size_t                       g_woa_cache_size;
static mtx_t                        g_woa_mtx;


static woa_hdr_t _Nullable _find_ww_locked(void* key)
{
    deque_for_each(&g_woa_table[hash_ptr(key) & WOA_HASH_CHAIN_MASK], struct woa_hdr, it,
        if (it->key == key) {
            return it;
        }
    )

    return NULL;
}

static woa_hdr_t _Nullable _acquire_woa_for_addr(void* key, bool bAllocIfNeeded)
{
    mtx_lock(&g_woa_mtx);
    woa_hdr_t wp = _find_ww_locked(key);
    
    if (wp == NULL && bAllocIfNeeded) {
        if (g_woa_cache_size > 0) {
            wp = (woa_hdr_t)deque_remove_first(&g_woa_cache);
            g_woa_cache_size--;
        }
        else {
            if (kalloc(sizeof(struct woa_hdr), (void**)&wp) != EOK) {
                mtx_unlock(&g_woa_mtx);
                return NULL;
            }
        }


        wp->qe = DEQUE_NODE_INIT;
        wp->wq = WAITQUEUE_INIT;
        wp->key = key;
        wp->use_count = 0;

        deque_add_last(&g_woa_table[hash_ptr(key) & WOA_HASH_CHAIN_MASK], &wp->qe);
    }


    if (wp) {
        wp->use_count++;
    }
    mtx_unlock(&g_woa_mtx);

    return wp;
}

static void _relinquish_woa(woa_hdr_t _Nonnull wp)
{
    mtx_lock(&g_woa_mtx);

    wp->use_count--;
    if (wp->use_count == 0) {
        // remove the ww from the hash table
        deque_remove(&g_woa_table[hash_ptr(wp->key) & WOA_HASH_CHAIN_MASK], &wp->qe);


        // put it on the cache queue if there's still space left; otherwise free it
        // for good
        if (g_woa_cache_size < WOA_CACHE_CAPACITY) {
            deque_add_first(&g_woa_cache, &wp->qe);
            g_woa_cache_size++;
        }
        else {
            wq_deinit(&wp->wq);
            kfree(wp);
        }
    }

    mtx_unlock(&g_woa_mtx);
}


SYSCALL_4(woa_wait, volatile atomic_int* _Nonnull addr, int expected, int flags, const nanotime_t* _Nonnull wtp)
{
    decl_try_err();
    woa_hdr_t wp = _acquire_woa_for_addr(pa->addr, true);
    if (wp == NULL) {
        return ENOMEM;
    }
    

    const ticks_t deadline = (pa->wtp) ? wq_calc_deadline(g_mono_clock, pa->flags, pa->wtp) : TICKS_MAX;
    const int sps = preempt_disable();
    for (;;) {
        if (vcpu_testabort_np() == EABORTED) {
            err = EABORTED;
            break;
        }
        if (atomic_int_load(pa->addr) != pa->expected) {
            break;
        }

        err = wq_wait_np(&wp->wq, deadline);
        if (err != EOK) {
            break;
        }
    }
    preempt_restore(sps);
    

    _relinquish_woa(wp);

    return err;
}

SYSCALL_2(woa_wakeup, volatile atomic_int* _Nonnull addr, int flags)
{
    if ((pa->flags & ~_WAKEUP_UMASK) != 0) {
        return EINVAL;
    }

    woa_hdr_t wp = _acquire_woa_for_addr(pa->addr, false);
    if (wp) {
        const int sps = preempt_disable();
        wq_wakeup_np(&wp->wq, pa->flags, 0);
        preempt_restore(sps);

        _relinquish_woa(wp);
    }

    return EOK;
}
