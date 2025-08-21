//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Driver.h"
#include <kern/Kalloc.h>
#include <Catalog.h>
#include <klib/Hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


struct dentry {
    SListNode           qe;
    CatalogId           dirId;
    HandlerRef _Nonnull driver;
    did_t               id;
};
typedef struct dentry* dentry_t;

struct matcher {
    SListNode           qe;
    drv_match_func_t    func;
    void* _Nullable     arg;
    iocat_t             cats[IOCAT_MAX];
};
typedef struct matcher* matcher_t;


#define HASH_CHAIN_COUNT 16
#define HASH_CHAIN_MASK  (HASH_CHAIN_COUNT - 1)

struct DriverManager {
    mtx_t               mtx;
    SList/*<dentry_t>*/ id_table[HASH_CHAIN_COUNT];  // did_t -> dentry_t
    SList/*<matcher_t*/ matchers;
};


DriverManagerRef gDriverManager;


errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverManagerRef self;

    try(kalloc_cleared(sizeof(struct DriverManager), (void**)&self));
    mtx_init(&self->mtx);

    for (size_t i = 0; i < HASH_CHAIN_COUNT; i++) {
        SList_Init(&self->id_table[i]);
    }


catch:
    *pOutSelf = self;
    return err;
}

FilesystemRef _Nonnull DriverManager_GetCatalog(DriverManagerRef _Nonnull self)
{
    return Catalog_GetFilesystem(gDriverCatalog);
}

errno_t DriverManager_Open(DriverManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Catalog_Open(gDriverCatalog, path, mode, pOutChannel);
}

errno_t DriverManager_HasDriver(DriverManagerRef _Nonnull self, const char* _Nonnull path)
{
    return Catalog_IsPublished(gDriverCatalog, path);
}

errno_t DriverManager_AcquireNodeForPath(DriverManagerRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp)
{
    return Catalog_AcquireNodeForPath(gDriverCatalog, path, rp);
}


// Returns a reference to the driver entry for driver id 'id'. NULL if such
// entry exists.
static dentry_t _Nullable _get_dentry_by_id(DriverManagerRef _Nonnull _Locked self, did_t id, SList* _Nullable * _Nullable pOutChain, dentry_t _Nullable * _Nullable pOutPrevEntry)
{
    SList* the_chain = &self->id_table[hash_scalar(id) & HASH_CHAIN_MASK];
    dentry_t prev_ep = NULL;

    SList_ForEach(the_chain, SListNode,
        dentry_t ep = (dentry_t)pCurNode;

        if (ep->id == id) {
            if (pOutChain) {
                *pOutChain = the_chain;
            }
            if (pOutPrevEntry) {
                *pOutPrevEntry = prev_ep;
            }
            return ep;
        }

        prev_ep = ep;
    );

    return NULL;
}

DriverRef _Nullable DriverManager_CopyDriverForId(DriverManagerRef _Nonnull self, did_t id)
{
    mtx_lock(&self->mtx);
    const dentry_t ep = _get_dentry_by_id(self, id, NULL, NULL);
    DriverRef dp = (ep) ? Object_RetainAs(ep->driver, Driver) : NULL;
    mtx_unlock(&self->mtx);

    return dp;
}

errno_t DriverManager_Publish(DriverManagerRef _Nonnull self, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId)
{
    decl_try_err();
    HandlerRef handler = NULL;
    ObjectRef catEntity = NULL;
    dentry_t ep = NULL;

    mtx_lock(&self->mtx);

    try(kalloc_cleared(sizeof(struct dentry), (void**)&ep));
    try(Catalog_PublishHandler(gDriverCatalog, de->dirId, de->name, de->uid, de->gid, de->perms, de->driver, de->arg, &ep->id));

    SList_InsertBeforeFirst(&self->id_table[hash_scalar(ep->id) & HASH_CHAIN_MASK], &ep->qe);
    ep->dirId = de->dirId;
    ep->driver = Object_RetainAs(de->driver, Handler);
    if (pOutId) {
        *pOutId = ep->id;
    }

catch:
    mtx_unlock(&self->mtx);

    if (err != EOK) {
        Object_Release(handler);
        kfree(ep);
    }

    return err;
}

// Removes the driver instance from the driver catalog.
void DriverManager_Unpublish(DriverManagerRef _Nonnull self, did_t id)
{
    SList* the_chain = NULL;
    dentry_t the_ep = NULL, prev_ep = NULL;
    HandlerRef driver = NULL;

    mtx_lock(&self->mtx);

    the_ep = _get_dentry_by_id(self, id, &the_chain, &prev_ep);
    if (the_ep == NULL) {
        mtx_unlock(&self->mtx);
        return;
    }

    if (the_ep->driver) {
        driver = the_ep->driver;
    }

    Catalog_Unpublish(gDriverCatalog, the_ep->dirId, id);
    SList_Remove(the_chain, &prev_ep->qe, &the_ep->qe);

    mtx_unlock(&self->mtx);
    

    Object_Release(driver);
    kfree(the_ep);
}

errno_t DriverManager_CreateDirectory(DriverManagerRef _Nonnull self, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutDirId)
{
    return Catalog_PublishFolder(gDriverCatalog, be->dirId, be->name, be->uid, be->gid, be->perms, pOutDirId);
}

errno_t DriverManager_RemoveDirectory(DriverManagerRef _Nonnull self, CatalogId dirId)
{
    if (dirId != kCatalogId_None) {
        return Catalog_Unpublish(gDriverCatalog, dirId, kCatalogId_None);
    }
    else {
        return EOK;
    }
}


errno_t DriverManager_GetMatches(DriverManagerRef _Nonnull self, const iocat_t* _Nonnull cats, HandlerRef* _Nonnull buf, size_t bufsiz)
{
    decl_try_err();
    size_t idx = 0;

    if (bufsiz == 0) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    for (size_t idx = 0; idx < HASH_CHAIN_COUNT; idx++) {
        SList_ForEach(&self->id_table[idx], SListNode,
            dentry_t dep = (dentry_t)pCurNode;
            
            if (instanceof(dep->driver, Driver) && Driver_MatchesAnyCategory((DriverRef)dep->driver, cats)) {
                if (idx >= bufsiz-1) {
                    err = ERANGE;
                    break;
                }

                buf[idx++] = Object_RetainAs(dep->driver, Handler);
            }
        );

        if (err != EOK) {
            break;
        }
    }
    buf[idx] = NULL;
    mtx_unlock(&self->mtx);

    return err;
}

errno_t DriverManager_StartMatching(DriverManagerRef _Nonnull self, const iocat_t* _Nonnull cats, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    decl_try_err();
    matcher_t pm;
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
    SList_InsertAfterLast(&self->matchers, &pm->qe);


    // Tell the matcher about all existing drivers
    for (size_t idx = 0; idx < HASH_CHAIN_COUNT; idx++) {
        SList_ForEach(&self->id_table[idx], SListNode,
            dentry_t dep = (dentry_t)pCurNode;
            
            if (instanceof(dep->driver, Driver) && Driver_MatchesAnyCategory((DriverRef)dep->driver, pm->cats)) {
                f(arg, dep->driver, IONOTIFY_STARTED);
            }
        );
    }

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void DriverManager_StopMatching(DriverManagerRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    SListNode* pprev = NULL;

    mtx_lock(&self->mtx);
    SList_ForEach(&self->matchers, SListNode,
        matcher_t p = (matcher_t)pCurNode;

        if (p->func == f && p->arg == arg) {
            SList_Remove(&self->matchers, pprev, &p->qe);
            break;
        }
        pprev = pCurNode;
    );
    mtx_unlock(&self->mtx);
}

static void _do_match_callouts(DriverManagerRef _Nonnull self, HandlerRef _Nonnull driver, int notify)
{
    mtx_lock(&self->mtx);
    SList_ForEach(&self->matchers, SListNode,
        matcher_t pm = (matcher_t)pCurNode;
            
        if (instanceof(driver, Driver) && Driver_MatchesAnyCategory((DriverRef)driver, pm->cats)) {
            pm->func(pm->arg, driver, notify);
        }
    );
    mtx_unlock(&self->mtx);
}

void DriverManager_OnDriverStarted(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, (HandlerRef)driver, IONOTIFY_STARTED);
}

void DriverManager_OnDriverStopping(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, (HandlerRef)driver, IONOTIFY_STOPPING);
}
