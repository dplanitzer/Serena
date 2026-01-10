//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Driver.h"
#include <Catalog.h>
#include <ext/hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


struct dentry {
    queue_node_t           qe;
    CatalogId           dirId;
    DriverRef _Nonnull  driver;
    did_t               id;
};
typedef struct dentry* dentry_t;

struct matcher {
    queue_node_t           qe;
    drv_match_func_t    func;
    void* _Nullable     arg;
    iocat_t             cats[IOCAT_MAX];
};
typedef struct matcher* matcher_t;


#define HASH_CHAIN_COUNT 16
#define HASH_CHAIN_MASK  (HASH_CHAIN_COUNT - 1)

struct DriverManager {
    mtx_t               mtx;
    queue_t/*<dentry_t>*/ id_table[HASH_CHAIN_COUNT];  // did_t -> dentry_t
    queue_t/*<matcher_t*/ matchers;
};


DriverManagerRef gDriverManager;


errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverManagerRef self;

    try(kalloc_cleared(sizeof(struct DriverManager), (void**)&self));
    mtx_init(&self->mtx);


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
static dentry_t _Nullable _get_dentry_by_id(DriverManagerRef _Nonnull _Locked self, did_t id, queue_t* _Nullable * _Nullable pOutChain, dentry_t _Nullable * _Nullable pOutPrevEntry)
{
    queue_t* the_chain = &self->id_table[hash_scalar(id) & HASH_CHAIN_MASK];
    dentry_t prev_ep = NULL;

    queue_for_each(the_chain, queue_node_t, it,
        dentry_t ep = (dentry_t)it;

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
    )

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

errno_t DriverManager_CreateEntry(DriverManagerRef _Nonnull self, DriverRef _Nonnull drv, CatalogId parentDirId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId)
{
    decl_try_err();
    DriverRef driver = NULL;
    ObjectRef catEntity = NULL;
    dentry_t ep = NULL;

    mtx_lock(&self->mtx);

    try(kalloc_cleared(sizeof(struct dentry), (void**)&ep));
    try(Catalog_PublishDriver(gDriverCatalog, parentDirId, de->name, de->uid, de->gid, de->perms, drv, de->arg, &ep->id));

    queue_add_first(&self->id_table[hash_scalar(ep->id) & HASH_CHAIN_MASK], &ep->qe);
    ep->dirId = parentDirId;
    ep->driver = Object_RetainAs(drv, Driver);
    if (pOutId) {
        *pOutId = ep->id;
    }

catch:
    mtx_unlock(&self->mtx);

    if (err != EOK) {
        Object_Release(driver);
        kfree(ep);
    }

    return err;
}

// Removes the driver instance from the driver catalog.
void DriverManager_RemoveEntry(DriverManagerRef _Nonnull self, did_t id)
{
    queue_t* the_chain = NULL;
    dentry_t the_ep = NULL, prev_ep = NULL;
    DriverRef driver = NULL;

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
    queue_remove(the_chain, &prev_ep->qe, &the_ep->qe);

    mtx_unlock(&self->mtx);
    

    Object_Release(driver);
    kfree(the_ep);
}

errno_t DriverManager_CreateDirectory(DriverManagerRef _Nonnull self, CatalogId parentDirId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutDirId)
{
    return Catalog_PublishFolder(gDriverCatalog, parentDirId, be->name, be->uid, be->gid, be->perms, pOutDirId);
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


errno_t DriverManager_GetMatches(DriverManagerRef _Nonnull self, const iocat_t* _Nonnull cats, DriverRef* _Nonnull buf, size_t bufsiz)
{
    decl_try_err();
    size_t idx = 0;

    if (bufsiz == 0) {
        return EINVAL;
    }

    mtx_lock(&self->mtx);
    for (size_t idx = 0; idx < HASH_CHAIN_COUNT; idx++) {
        queue_for_each(&self->id_table[idx], queue_node_t, it,
            dentry_t dep = (dentry_t)it;
            
            if (instanceof(dep->driver, Driver) && Driver_HasSomeCategories((DriverRef)dep->driver, cats)) {
                if (idx >= bufsiz-1) {
                    err = ERANGE;
                    break;
                }

                buf[idx++] = Object_RetainAs(dep->driver, Driver);
            }
        )

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
    queue_add_last(&self->matchers, &pm->qe);


    // Tell the matcher about all existing drivers
    for (size_t idx = 0; idx < HASH_CHAIN_COUNT; idx++) {
        queue_for_each(&self->id_table[idx], queue_node_t, it,
            dentry_t dep = (dentry_t)it;
            
            if (instanceof(dep->driver, Driver) && Driver_HasSomeCategories((DriverRef)dep->driver, pm->cats)) {
                f(arg, dep->driver, IONOTIFY_STARTED);
            }
        )
    }

catch:
    mtx_unlock(&self->mtx);

    return err;
}

void DriverManager_StopMatching(DriverManagerRef _Nonnull self, drv_match_func_t _Nonnull f, void* _Nullable arg)
{
    queue_node_t* pprev = NULL;

    mtx_lock(&self->mtx);
    queue_for_each(&self->matchers, queue_node_t, it,
        matcher_t p = (matcher_t)it;

        if (p->func == f && p->arg == arg) {
            queue_remove(&self->matchers, pprev, &p->qe);
            break;
        }
        pprev = it;
    )
    mtx_unlock(&self->mtx);
}

static void _do_match_callouts(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver, int notify)
{
    mtx_lock(&self->mtx);
    queue_for_each(&self->matchers, queue_node_t, it,
        matcher_t pm = (matcher_t)it;
            
        if (Driver_HasSomeCategories((DriverRef)driver, pm->cats)) {
            pm->func(pm->arg, driver, notify);
        }
    )
    mtx_unlock(&self->mtx);
}

void DriverManager_OnDriverStarted(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, driver, IONOTIFY_STARTED);
}

void DriverManager_OnDriverStopping(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    _do_match_callouts(self, driver, IONOTIFY_STOPPING);
}
