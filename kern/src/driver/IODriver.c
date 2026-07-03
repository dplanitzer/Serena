//
//  IODriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "IODriver.h"
#include "IORegistry.h"
#include <assert.h>
#include <string.h>
#include <ext/atomic.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kpi/file.h>
#include <sched/cnd.h>

// Lifecycle states
#define kIODriverState_Initialized  0
#define kIODriverState_Launching    1
#define kIODriverState_Launched     2
#define kIODriverState_Terminating  3
#define kIODriverState_Terminated   4


static cnd_t                g_await_termination;
static volatile atomic_int  g_next_driver_id = {.value = 1};


static int _set_state(IODriverRef _Nonnull self, int newState)
{
    mtx_lock(&self->mtx);
    const int oldState = self->state;
    
    if (newState > oldState) {
        self->state = newState;
    }
    mtx_unlock(&self->mtx);

    return oldState;
}

static int _get_state(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const int curState = self->state;
    mtx_unlock(&self->mtx);

    return curState;
}


errno_t IODriver_Create(Class* _Nonnull pClass, const iocat_t* _Nonnull cats, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    IODriverRef self;

    err = Object_Create(pClass, 0, (void**)&self);
    if (err == EOK) {
        mtx_init(&self->mtx);

        self->id = atomic_int_fetch_add(&g_next_driver_id, 1);
        self->cats = cats;

        *pOutSelf = self;
    }

    return err;
}


void IODriver_deinit(IODriverRef _Nonnull self)
{
    if (_get_state(self) < kIODriverState_Terminated) {
        abort();
        /* NOT REACHED */
    }

    mtx_deinit(&self->mtx);
}


//
// MARK: -
// MARK: API
//

errno_t IODriver_attachProvider(IODriverRef _Nonnull self, IODriverRef _Nonnull provider)
{
    if (_get_state(provider) >= kIODriverState_Terminating) {
        return ENODEV;
    }

    IORegistry_AttachProvider(gIORegistry, provider, self);
    return EOK;
}

void IODriver_detachProvider(IODriverRef _Nonnull self, IODriverRef _Nonnull provider)
{
    IORegistry_DetachProvider(gIORegistry, self);
}


errno_t IODriver_start(IODriverRef _Nonnull self)
{
    return EOK;
}


bool IODriver_stop(IODriverRef _Nonnull self)
{
    return true;
}


errno_t IODriver_getDFSInfo(IODriverRef _Nonnull self, IODFSInfo* _Nonnull info)
{
    return ENOTSUP;
}

static errno_t _create_dfs_entry(IODriverRef _Nonnull self)
{
    decl_try_err();
    static mtx_t g_dfs_mtx;
    static IODFSInfo g_dfs_info;
    static devfs_entry_t g_dfs_entry;
    static char g_dfs_name[kIODFSMaxName + 10];

    mtx_lock(&g_dfs_mtx);

    // Get the devfs entry information
    try(IODriver_GetDFSInfo(self, &g_dfs_info));


    const size_t len = strlen(g_dfs_info.name);
    if (len >= kIODFSMaxName - 1) {
        throw(EINVAL);
    }


    // Figure out where we should insert a number to make teh entry unique if needed
    const char* p = strchr(g_dfs_info.name, '$');
    const char* dollar_p = (p) ? p : g_dfs_info.name + len;
    const size_t base_len = dollar_p - g_dfs_info.name;


    g_dfs_entry.name = g_dfs_name;
    g_dfs_entry.resource = (ObjectRef)self;
    g_dfs_entry.func = g_dfs_info.func;
    g_dfs_entry.uid = g_dfs_info.uid;
    g_dfs_entry.gid = g_dfs_info.gid;
    g_dfs_entry.perms = g_dfs_info.perms;


    // Try to add the entry. This may result in an EEXIST error. Make the entry
    // unique by adding an integer at the '$' position. 
    for (int i = 1; i <= 50; i++) {
        g_dfs_name[0] = '\0';

        strncat(g_dfs_name, g_dfs_info.name, base_len);
        if (err == EEXIST) {
            itoa(i, &g_dfs_name[base_len], 10);
        }
        if (base_len < len) {
            strcat(g_dfs_name, dollar_p + 1);
        }

        err = devfs_add(&g_dfs_entry, &self->devfs_hnd);
        if (err != EEXIST) {
            break;
        }
    }

catch:
    mtx_unlock(&g_dfs_mtx);

    return err;
}

void _delete_dfs_entry(IODriverRef _Nonnull self)
{
    if (self->devfs_hnd > 0) {
        devfs_remove(self->devfs_hnd);
        self->devfs_hnd = 0;
    }
}


void IODriver_onLaunched(IODriverRef _Nonnull self)
{
}

errno_t IODriver_launch(IODriverRef _Nonnull self, IODriverRef _Nullable provider)
{
    decl_try_err();

    // Protect against restart attempts. Concurrent launch() and terminate() are
    // not allowed anyway.
    switch (_set_state(self, kIODriverState_Launching)) {
        case kIODriverState_Launching:
        case kIODriverState_Launched:
            return EBUSY;

        case kIODriverState_Terminating:
        case kIODriverState_Terminated:
            return ENODEV;

        default:
            break;
    }


    if (provider) {
        err = IODriver_AttachProvider(self, provider);
        if (err != EOK) {
            return err;
        }
    }


    err = IODriver_Start(self);
    if (err == EOK) {
        IORegistry_RegisterDriver(gIORegistry, self);
        _create_dfs_entry(self);
        _set_state(self, kIODriverState_Launched);
    }
    else if (provider) {
        IODriver_DetachProvider(self, provider);
    }


    // Do the matching callouts after we've dropped our lock to avoid deadlocks.
    if (err == EOK) {
        IODriver_OnLaunched(self);
        IORegistry_Notify(gIORegistry, self, IOMATCH_STARTED);
    }

    return err;
}


void IODriver_onTerminating(IODriverRef _Nonnull self)
{
}

static void _terminate_client_drivers(IODriverRef _Nonnull self)
{
    decl_try_err();
    IOIterator iter;

    err = IORegistry_CopyClientDrivers(gIORegistry, self, &iter);
    if (err == EOK) {
        while (IOIterator_HasNext(&iter)) {
            IODriver_Terminate(IOIterator_GetNext(&iter));
        }
        IOIterator_Destroy(&iter);
    }
}

static void _finalize_termination(IODriverRef _Nonnull self)
{
    IODriver_DetachProvider(self, self->provider);
    _set_state(self, kIODriverState_Terminated);
}

void IODriver_terminate(IODriverRef _Nonnull self)
{
    switch (_set_state(self, kIODriverState_Terminating)) {
        case kIODriverState_Terminating:
        case kIODriverState_Terminated:
            return;

        case kIODriverState_Initialized:
        case kIODriverState_Launching:
            abort();    //XXX potentially rethink, though good enough for now

        default:
            break;
    }


    IODriver_OnTerminating(self);
    _terminate_client_drivers(self);


    IORegistry_Notify(gIORegistry, self, IOMATCH_STOPPING);
    _delete_dfs_entry(self);

    Object_Retain(self);
    IORegistry_DeregisterDriver(gIORegistry, self);


    if (IODriver_Stop(self)) {
        _finalize_termination(self);
        Object_Release(self);
    }
    // else we'll stay in terminating state until a call to TerminationCompleted()
    // since termination is async
}

void IODriver_AwaitTermination(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    while (self->state < kIODriverState_Terminated) {
        cnd_wait(&g_await_termination, &self->mtx);
    }
    mtx_unlock(&self->mtx);

    // Balance the tmp retain we did before removing ourselves from the I/O
    // registry in IODriver_Terminate().
    Object_Release(self);
}

void IODriver_TerminationCompleted(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    if (self->state == kIODriverState_Terminating) {
        _finalize_termination(self);
        cnd_broadcast(&g_await_termination);
    }
    mtx_unlock(&self->mtx);
}


bool IODriver_isOpen(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    const bool r = IODriver_DoIsOpen(self);
    mtx_unlock(&self->mtx);

    return r;
}

errno_t IODriver_open(IODriverRef _Nonnull self, fd_flags_t flags)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    if (self->state == kIODriverState_Launched) {
        err = IODriver_DoOpen(self, flags);
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void IODriver_close(IODriverRef _Nonnull self)
{
    mtx_lock(&self->mtx);
    IODriver_DoClose(self);
    mtx_unlock(&self->mtx);
}


bool IODriver_isExclusive(IODriverRef _Nonnull self)
{
    return true;
}

errno_t IODriver_doOpen(IODriverRef _Nonnull _Locked self, fd_flags_t flags)
{
    if (IODriver_IsExclusive(self) && self->open_cnt > 0) {
        return EBUSY;
    }
    else {
        self->open_cnt++;
        return EOK;
    }
}

void IODriver_doClose(IODriverRef _Nonnull _Locked self)
{
    assert(self->open_cnt > 0);
    self->open_cnt--;
}

bool IODriver_doIsOpen(IODriverRef _Nonnull self)
{
    return (self->open_cnt > 0) ? true : false;
}


bool IODriver_HasCategory(IODriverRef _Nonnull self, iocat_t cat)
{
    // Category info is constant - no need to take a lock here
    const iocat_t* p = self->cats;

    while (*p != cat && *p != IOCAT_END) {
        p++;
    }

    return (*p == cat) ? true : false;
}

bool IODriver_HasSomeCategories(IODriverRef _Nonnull self, const iocat_t* _Nonnull cats)
{
    while (*cats != IOCAT_END) {
        if (IODriver_HasCategory(self, *cats)) {
            return true;
        }

        cats++;
    }

    return false;
}


class_func_defs(IODriver, Object,
override_func_def(deinit, IODriver, Object)
func_def(attachProvider, IODriver)
func_def(detachProvider, IODriver)
func_def(start, IODriver)
func_def(stop, IODriver)
func_def(launch, IODriver)
func_def(onLaunched, IODriver)
func_def(terminate, IODriver)
func_def(onTerminating, IODriver)
func_def(getDFSInfo, IODriver)
func_def(isOpen, IODriver)
func_def(open, IODriver)
func_def(close, IODriver)
func_def(isExclusive, IODriver)
func_def(doIsOpen, IODriver)
func_def(doOpen, IODriver)
func_def(doClose, IODriver)
);
