//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "Driver.h"
#include <handler/Handler.h>
#include <kern/Kalloc.h>
#include <Catalog.h>
#include <klib/Hash.h>
#include <kern/kalloc.h>
#include <sched/mtx.h>


struct dentry {
    SListNode   qe;
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
static dentry_t _Nullable _get_dentry_by_id(DriverManagerRef _Nonnull _Locked self, did_t id)
{
    SList_ForEach(&self->id_table[hash_scalar(id) & HASH_CHAIN_MASK], ListNode,
        dentry_t ep = (dentry_t)pCurNode;

        if (ep->id == id) {
            return ep;
        }
    );

    return NULL;
}

DriverRef _Nullable DriverManager_CopyDriverForId(DriverManagerRef _Nonnull self, did_t id)
{
    mtx_lock(&self->mtx);
    const dentry_t ep = _get_dentry_by_id(self, id);
    DriverRef dp = (ep) ? Object_RetainAs(ep->driver, Driver) : NULL;
    mtx_unlock(&self->mtx);

    return dp;
}

errno_t DriverManager_Publish(DriverManagerRef _Nonnull self, const DriverEntry* _Nonnull de)
{
    decl_try_err();
    dentry_t ep = NULL;

    mtx_lock(&self->mtx);

    if ((de->driver && de->driver->id != 0) || (de->handler && de->handler->id != 0)) {
        throw(EBUSY);
    }

    try(kalloc_cleared(sizeof(struct dentry), (void**)&ep));
    try(Catalog_PublishDriver(gDriverCatalog, de->dirId, de->name, de->uid, de->gid, de->perms, (ObjectRef)de->driver, de->arg, &ep->id));

    SList_InsertBeforeFirst(&self->id_table[hash_scalar(ep->id) & HASH_CHAIN_MASK], &ep->qe);
    ep->driver = Object_RetainAs(de->driver, Driver);
    de->driver->id = ep->id;

catch:
    mtx_unlock(&self->mtx);

    if (err != EOK) {
        kfree(ep);
    }

    return err;
}

// Removes the driver instance from the driver catalog.
void DriverManager_Unpublish(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    dentry_t the_ep = NULL;

    mtx_lock(&self->mtx);
    const id = driver->id;

    if (id == 0) {
        mtx_unlock(&self->mtx);
        return;
    }

    Catalog_Unpublish(gDriverCatalog, Driver_GetParentDirectoryId(driver), id);
    
    dentry_t prev_ep = NULL;
    SList* id_chain = &self->id_table[hash_scalar(id) & HASH_CHAIN_MASK];
    SList_ForEach(id_chain, ListNode,
        dentry_t ep = (dentry_t)pCurNode;

        if (ep->id == id) {
            SList_Remove(id_chain, &prev_ep->qe, &ep->qe);
            the_ep = ep;
            break;
        }
        prev_ep = ep;
    );

    driver->id = 0;

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
