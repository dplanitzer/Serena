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
#include <driver/Driver.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


#define drv_from_ioreg_qe(__ptr) \
(DriverRef)deque_node_as(__ptr, ioreg_qe, Driver)


struct matcher {
    queue_node_t        qe;
    drv_match_func_t    func;
    void* _Nullable     arg;
    iocat_t             cats[IOCAT_MAX];
};
typedef struct matcher* matcher_t;


typedef struct IORegistry {
    mtx_t                       mtx;
    deque_t/*Driver*/           drivers;
    queue_t/*<matcher_t*/       matchers;
    size_t                      driver_count;
} IORegistry;


IORegistryRef gIORegistry;


errno_t IORegistry_Create(IORegistryRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IORegistryRef self = NULL;

    try(kalloc_cleared(sizeof(IORegistry), (void**) &self));
    mtx_init(&self->mtx);

catch:
    *pOutSelf = self;
    return err;
}

static void _do_match_callouts(IORegistryRef _Nonnull _Locked self, DriverRef _Nonnull driver, int notify)
{
    queue_for_each(&self->matchers, struct matcher, it,
        if (Driver_HasSomeCategories((DriverRef)driver, it->cats)) {
            it->func(it->arg, driver, notify);
        }
    )
}

static errno_t _copy_matching_drivers(IORegistryRef _Nonnull _Locked self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers)
{
    decl_try_err();
    DriverRef* drivers = NULL;
    size_t idx = 0;

    try(kalloc(sizeof(DriverRef) * (self->driver_count + 1), (void**)&drivers));

    deque_for_each(&self->drivers, deque_node_t, it,
        DriverRef cdp = drv_from_ioreg_qe(it);

        if (Driver_HasSomeCategories(cdp, cats)) {
            drivers[idx++] = Object_Retain(cdp);
        }
    )
    drivers[idx] = NULL;

catch:

    *pOutDrivers = drivers;
    return err;
}

static DriverRef _Nullable _copy_best_matching_driver(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    DriverRef drv = NULL;

    deque_for_each(&self->drivers, deque_node_t, it,
        DriverRef cdp = drv_from_ioreg_qe(it);

        if (Driver_HasSomeCategories(cdp, cats)) {
            drv = Object_Retain(cdp);
            break;
        }
    )

catch:
    return drv;
}


void IORegistry_RegisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv)
{
    mtx_lock(&self->mtx);
    assert(drv->ioreg_qe.next == NULL && drv->ioreg_qe.prev == NULL);
    deque_add_last(&self->drivers, &drv->ioreg_qe);
    self->driver_count++;

    _do_match_callouts(self, drv, IONOTIFY_STARTED);

    mtx_unlock(&self->mtx);
}

void IORegistry_DeregisterDriver(IORegistryRef _Nonnull self, DriverRef _Nonnull drv)
{
    mtx_lock(&self->mtx);

    _do_match_callouts(self, drv, IONOTIFY_STOPPING);

    self->driver_count--;
    deque_remove(&self->drivers, &drv->ioreg_qe);
    mtx_unlock(&self->mtx);
}

errno_t IORegistry_CopyMatchingDrivers(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nullable * _Nonnull pOutDrivers)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    err = _copy_matching_drivers(self, cats, pOutDrivers);
    mtx_unlock(&self->mtx);

    return err;
}

DriverRef _Nullable IORegistry_CopyBestMatchingDriver(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    mtx_lock(&self->mtx);
    DriverRef drv = _copy_best_matching_driver(self, cats);
    mtx_unlock(&self->mtx);

    return drv;
}

errno_t IORegistry_StartMatching(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    decl_try_err();
    DriverRef* drivers = NULL;
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
    try(_copy_matching_drivers(self, cats, &drivers));
    i = 0;
    while (drivers[i]) {
        f(arg, drivers[i], IONOTIFY_STARTED);
        i++;
    }
    kfree(drivers);

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void IORegistry_StopMatching(IORegistryRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg)
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

errno_t IORegistry_OpenBestMatch(IORegistryRef _Nonnull self, const iocat_t* _Nonnull cats, fd_flags_t oflags, DriverRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    DriverRef drv;

    mtx_lock(&self->mtx);
    drv = _copy_best_matching_driver(self, cats);
    if (drv) {
        err = Driver_Open(drv, oflags);

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
