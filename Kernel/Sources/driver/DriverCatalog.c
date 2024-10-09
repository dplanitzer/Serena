//
//  DriverCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DriverCatalog.h"
#include <dispatcher/Lock.h>

#define MAX_DRIVER_NAME_LENGTH  10

typedef struct DriverEntry {
    SListNode           node;
    DriverRef _Nonnull  instance;
    char                name[MAX_DRIVER_NAME_LENGTH + 1];
} DriverEntry;


typedef struct DriverCatalog {
    SList   drivers;
    Lock    lock;
} DriverCatalog;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DriverEntry
////////////////////////////////////////////////////////////////////////////////

static void DriverEntry_Destroy(DriverEntry* _Nullable self)
{
    if (self) {
        SListNode_Deinit(&self->node);

        Object_Release(self->instance);
        self->instance = NULL;
        
        kfree(self);
    }
}

// Adopts the given driver. Meaning that this initializer does not take out an
// extra strong reference to the driver. It assumes that it should take ownership
// of the provided strong reference.
static errno_t DriverEntry_Create(const char* _Nonnull name, DriverRef _Consuming _Nonnull driver, DriverEntry* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverEntry* self = NULL;

    try(kalloc_cleared(sizeof(DriverEntry), (void**) &self));
    SListNode_Init(&self->node);
    self->instance = driver;
    String_CopyUpTo(self->name, name, MAX_DRIVER_NAME_LENGTH);

    *pOutSelf = self;
    return EOK;

catch:
    DriverEntry_Destroy(self);
    *pOutSelf = NULL;
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DriverCatalog
////////////////////////////////////////////////////////////////////////////////

DriverCatalogRef _Nonnull  gDriverCatalog;

errno_t DriverCatalog_Create(DriverCatalogRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverCatalog* self;
    
    try(kalloc_cleared(sizeof(DriverCatalog), (void**) &self));
    Lock_Init(&self->lock);
    SList_Init(&self->drivers);

    *pOutSelf = self;
    return EOK;

catch:
    DriverCatalog_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DriverCatalog_Destroy(DriverCatalogRef _Nullable self)
{
    if (self) {
        SListNode* pCurEntry = self->drivers.first;

        while (pCurEntry != NULL) {
            SListNode* pNextEntry = pCurEntry->next;

            DriverEntry_Destroy((DriverEntry*)pCurEntry);
            pCurEntry = pNextEntry;
        }
        
        SList_Deinit(&self->drivers);
        Lock_Deinit(&self->lock);
        kfree(self);
    }
}

static DriverEntry* _Nullable _DriverCatalog_GetDriverEntry(DriverCatalogRef _Locked _Nonnull self, const char* _Nonnull name, DriverEntry* _Nullable * _Nullable pOutPrevEntry)
{
    DriverEntry* prevEntry = NULL;
    DriverEntry* theEntry = NULL;

    SList_ForEach(&self->drivers, DriverEntry, {
        if (String_Equals(pCurNode->name, name)) {
            theEntry = pCurNode;
            break;
        }

        prevEntry = pCurNode;
    })

    if (pOutPrevEntry) {
        *pOutPrevEntry = prevEntry;
    }
    return theEntry;
}

errno_t DriverCatalog_RegisterDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Consuming _Nonnull driver)
{
    decl_try_err();
    DriverEntry* entry = NULL;

    Lock_Lock(&self->lock);
    err = DriverEntry_Create(name, driver, &entry);
    if (err == EOK) {
        SList_InsertAfterLast(&self->drivers, &entry->node);
    }
    Lock_Unlock(&self->lock);

    return err;
}

void DriverCatalog_UnregisterDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull name)
{
    DriverEntry* prevEntry = NULL;
    DriverEntry* entry = NULL;

    Lock_Lock(&self->lock);
    entry = _DriverCatalog_GetDriverEntry(self, name, &prevEntry);
    if (entry) {
        SList_Remove(&self->drivers, &prevEntry->node, &entry->node);
    }
    Lock_Unlock(&self->lock);
}

DriverRef DriverCatalog_GetDriverForName(DriverCatalogRef _Nonnull self, const char* name)
{
    Lock_Lock(&self->lock);
    DriverEntry* entry = _DriverCatalog_GetDriverEntry(self, name, NULL);
    Lock_Unlock(&self->lock);

    return (entry) ? entry->instance : NULL;
}
