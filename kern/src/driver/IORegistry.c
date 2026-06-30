//
//  IORegistry.c
//  kernel
//
//  Created by Dietmar Planitzer on 6/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//


#include "IORegistry.h"
#include <assert.h>
#include <string.h>
#include <driver/IODriver.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define drv_from_ioreg_qe(__ptr) \
(IODriverRef)deque_node_as(__ptr, ioreg_qe, IODriver)


struct matcher {
    queue_node_t        qe;
    IOMatchCallback     func;
    void* _Nullable     arg;
    iocat_t             cats[IOCAT_MAX];
};
typedef struct matcher* matcher_t;


struct IORegistry {
    mtx_t                   mtx;
    deque_t/*IODriver*/     drivers;
    queue_t/*<matcher_t*/   matchers;
    size_t                  driver_count;
};


static struct IORegistry    g_io_reg;
IORegistryRef               gIORegistry;


void IORegistry_Init(void)
{
    mtx_init(&g_io_reg.mtx);

    gIORegistry = &g_io_reg;
}

static void _do_match_callouts(IORegistryRef _Nonnull _Locked self, IODriverRef _Nonnull driver, int notify)
{
    queue_for_each(&self->matchers, struct matcher, it,
        if (IODriver_HasSomeCategories((IODriverRef)driver, it->cats)) {
            it->func(it->arg, driver, notify);
        }
    )
}

static errno_t _create_matching_drivers_iterator(IORegistryRef _Nonnull _Locked self, const iocat_t* _Nonnull cats, IOIterator* _Nonnull iter)
{
    decl_try_err();

    iter->drivers = NULL;
    iter->count = 0;
    iter->idx = 0;

    try(kalloc(sizeof(IODriverRef) * self->driver_count, (void**)&iter->drivers));

    deque_for_each(&self->drivers, deque_node_t, it,
        IODriverRef cdp = drv_from_ioreg_qe(it);

        if (IODriver_HasSomeCategories(cdp, cats)) {
            iter->drivers[iter->count++] = Object_Retain(cdp);
        }
    )

catch:
    return err;
}

static IODriverRef _Nullable _copy_best_matching_driver(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    IODriverRef drv = NULL;

    deque_for_each(&self->drivers, deque_node_t, it,
        IODriverRef cdp = drv_from_ioreg_qe(it);

        if (IODriver_HasSomeCategories(cdp, cats)) {
            drv = Object_Retain(cdp);
            break;
        }
    )

catch:
    return drv;
}


void IORegistry_RegisterDriver(IORegistryRef _Nonnull self, IODriverRef _Nonnull drv)
{
    mtx_lock(&self->mtx);
    Object_Retain(drv);
    assert(drv->ioreg_qe.next == NULL && drv->ioreg_qe.prev == NULL);
    deque_add_last(&self->drivers, &drv->ioreg_qe);
    self->driver_count++;

    _do_match_callouts(self, drv, IOMATCH_STARTED);

    mtx_unlock(&self->mtx);
}

void IORegistry_DeregisterDriver(IORegistryRef _Nonnull self, IODriverRef _Nonnull drv)
{
    mtx_lock(&self->mtx);

    _do_match_callouts(self, drv, IOMATCH_STOPPING);

    self->driver_count--;
    deque_remove(&self->drivers, &drv->ioreg_qe);
    Object_Release(drv);
    mtx_unlock(&self->mtx);
}

IODriverRef _Nullable IORegistry_CopyDriverWithId(IORegistryRef _Nonnull self, did_t id)
{
    IODriverRef drv = NULL;

    mtx_lock(&self->mtx);
    deque_for_each(&self->drivers, deque_node_t, it,
        IODriverRef cdp = drv_from_ioreg_qe(it);

        if (IODriver_GetId(cdp) == id) {
            drv = Object_Retain(cdp);
            break;
        }
    )
    mtx_unlock(&self->mtx);

    return drv;
}

errno_t IORegistry_CopyClientDrivers(IORegistryRef _Nonnull self, IODriverRef _Nonnull provider, IOIterator* _Nonnull iter)
{
    decl_try_err();

    iter->drivers = NULL;
    iter->count = 0;
    iter->idx = 0;

    mtx_lock(&self->mtx);
    try(kalloc(sizeof(IODriverRef) * self->driver_count, (void**)&iter->drivers));

    deque_for_each(&self->drivers, deque_node_t, it,
        IODriverRef cdp = drv_from_ioreg_qe(it);

        if (cdp->provider == provider) {
            iter->drivers[iter->count++] = Object_Retain(cdp);
        }
    )

catch:
    mtx_unlock(&self->mtx);

    return err;
}

errno_t IORegistry_CopyMatchingDrivers(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, IOIterator* _Nonnull iter)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = _create_matching_drivers_iterator(self, cats, iter);
    mtx_unlock(&self->mtx);

    return err;
}

IODriverRef _Nullable IORegistry_CopyBestMatchingDriver(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    mtx_lock(&self->mtx);
    IODriverRef drv = _copy_best_matching_driver(self, cats);
    mtx_unlock(&self->mtx);

    return drv;
}

errno_t IORegistry_StartMatching(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, IOMatchCallback _Nonnull f, void* _Nullable arg)
{
    decl_try_err();
    IOIterator iter;
    matcher_t pm = NULL;
    int i = 0;

    mtx_lock(&self->mtx);

    // Create a matcher entry
    try(kalloc_cleared(sizeof(struct matcher), (void**)&pm));
    pm->func = f;
    pm->arg = arg;
    while (i < IOCAT_MAX-1 && cats[i] != IOCAT_END) {
        pm->cats[i] = cats[i];
        i++;
    }
    pm->cats[i] = IOCAT_END;
    queue_add_last(&self->matchers, &pm->qe);


    // Tell the matcher about all existing drivers
    try(_create_matching_drivers_iterator(self, cats, &iter));
    while (IOIterator_HasNext(&iter)) {
        f(arg, IOIterator_GetNext(&iter), IOMATCH_STARTED);
    }
    IOIterator_Destroy(&iter);

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void IORegistry_StopMatching(IORegistryRef _Nonnull self, IOMatchCallback _Nonnull f, void* _Nullable arg)
{
    matcher_t pprev = NULL;

    mtx_lock(&self->mtx);
    queue_for_each(&self->matchers, struct matcher, it,
        if (it->func == f && it->arg == arg) {
            queue_remove(&self->matchers, &pprev->qe, &it->qe);
            break;
        }
        pprev = it;
    )
    mtx_unlock(&self->mtx);
}

errno_t IORegistry_OpenBestMatch(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, fd_flags_t oflags, IODriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    IODriverRef drv;

    mtx_lock(&self->mtx);
    drv = _copy_best_matching_driver(self, cats);
    if (drv) {
        err = IODriver_Open(drv, oflags);

        if (err == EOK) {
            *pOutDriver = drv;
        }
        else {
            Object_Release(drv);
        }
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void IORegistry_AttachProvider(IORegistryRef _Nonnull self, IODriverRef _Nonnull provider, IODriverRef _Nonnull client)
{
    mtx_lock(&self->mtx);
    client->provider = Object_Retain(provider);
    mtx_unlock(&self->mtx);
}

void IORegistry_DetachProvider(IORegistryRef _Nonnull self, IODriverRef _Nonnull client)
{
    mtx_lock(&self->mtx);
    if (client->provider) {
        Object_Release(client->provider);
        client->provider = NULL;
    }
    mtx_unlock(&self->mtx);
}


//
// IOIterator
//

void IOIterator_Destroy(IOIterator* _Nullable iter)
{
    if (iter) {
        for (size_t i = 0; i < iter->count; i++) {
            Object_Release(iter->drivers[i]);
        }

        kfree(iter->drivers);
    }
}
