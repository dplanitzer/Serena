//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Driver.h"
#include <handler/DriverHandler.h>
#include <kern/Kalloc.h>
#include <Catalog.h>
#include <klib/Hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


struct dentry {
    SListNode   qe;
    CatalogId   dirId;
    HandlerRef  handler;
    DriverRef   driver;
    did_t       id;
};
typedef struct dentry* dentry_t;


#define HASH_CHAIN_COUNT 16
#define HASH_CHAIN_MASK  (HASH_CHAIN_COUNT - 1)

struct DriverManager {
    mtx_t               mtx;
    SList/*<dentry_t>*/ id_table[HASH_CHAIN_COUNT];  // did_t -> dentry_t
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

    SList_ForEach(the_chain, ListNode,
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

static errno_t _create_std_handler(IOCategory category, DriverRef _Nonnull driver, HandlerRef _Nullable * _Nonnull pOutHandler)
{
    Class* cl;

    switch (category) {
        case kIOCategory_Keyboard:  cl = class(DriverHandler); break;
        default: return EINVAL;
    }

    return DriverHandler_Create(cl, driver, pOutHandler);
}

errno_t DriverManager_Publish(DriverManagerRef _Nonnull self, const DriverEntry* _Nonnull de)
{
    decl_try_err();
    HandlerRef handler = NULL;
    ObjectRef catEntity = NULL;
    dentry_t ep = NULL;

    mtx_lock(&self->mtx);

    if ((de->driver && de->driver->id != 0) || (de->handler && de->handler->id != 0)) {
        throw(EBUSY);
    }

    try(kalloc_cleared(sizeof(struct dentry), (void**)&ep));

    if (de->handler) {
        handler = Object_RetainAs(de->handler, Handler);
    }
    else if (de->category > 0) {
        try(_create_std_handler(de->category, de->driver, &handler));
    }


    if (handler) {
        catEntity = (ObjectRef)handler;
    }
    else {
        catEntity = (ObjectRef)de->driver;
    }

    try(Catalog_PublishDriver(gDriverCatalog, de->dirId, de->name, de->uid, de->gid, de->perms, catEntity, de->arg, &ep->id));

    SList_InsertBeforeFirst(&self->id_table[hash_scalar(ep->id) & HASH_CHAIN_MASK], &ep->qe);
    ep->dirId = de->dirId;
    if (handler) {
        ep->handler = handler;
        ep->handler->id = ep->id;
    }
    if (de->driver) {
        ep->driver = Object_RetainAs(de->driver, Driver);
        de->driver->id = ep->id;
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
    DriverRef driver = NULL;
    HandlerRef handler = NULL;

    mtx_lock(&self->mtx);

    the_ep = _get_dentry_by_id(self, id, &the_chain, &prev_ep);
    if (the_ep == NULL) {
        mtx_unlock(&self->mtx);
        return;
    }

    if (the_ep->handler) {
        the_ep->handler->id = 0;
        handler = the_ep->handler;
    }
    if (the_ep->driver) {
        the_ep->driver->id = 0;
        driver = the_ep->driver;
    }

    Catalog_Unpublish(gDriverCatalog, the_ep->dirId, id);
    SList_Remove(the_chain, &prev_ep->qe, &the_ep->qe);

    mtx_unlock(&self->mtx);
    

    Object_Release(handler);
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
