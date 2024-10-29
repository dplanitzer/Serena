//
//  DriverCatalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DriverCatalog.h"
#include <dispatcher/Lock.h>

#define MAX_DRIVER_NAME_LENGTH      10
#define DRIVER_ID_HASH_CHAINS_COUNT 8
#define driver_id_hash(__id)        (~(DRIVER_ID_HASH_CHAINS_COUNT-1) & (__id))


typedef struct DriverEntry {
    ListNode            node;
    DriverRef _Nonnull  instance;
    DriverId            driverId;
    char                name[MAX_DRIVER_NAME_LENGTH + 1];
} DriverEntry;


typedef struct DriverIdHashEntry {
    ListNode                        node;
    struct DriverEntry* _Nonnull    entry;
} DriverIdHashEntry;


typedef struct DriverCatalog {
    Lock    lock;
    List    drivers;
    List    driversByDriverId[DRIVER_ID_HASH_CHAINS_COUNT];
} DriverCatalog;


static void _DriverCatalog_DestroyDriverTable(DriverCatalogRef _Nonnull self);
static void _DriverCatalog_DestroyDriverIdHashTable(DriverCatalogRef _Nonnull self);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DriverEntry
////////////////////////////////////////////////////////////////////////////////

static void DriverEntry_Destroy(DriverEntry* _Nullable self)
{
    if (self) {
        Object_Release(self->instance);
        self->instance = NULL;

        ListNode_Deinit(&self->node);
        
        kfree(self);
    }
}

// Adopts the given driver. Meaning that this initializer does not take out an
// extra strong reference to the driver. It assumes that it should take ownership
// of the provided strong reference.
static errno_t DriverEntry_Create(const char* _Nonnull name, DriverId driverId, DriverRef _Consuming _Nonnull driver, DriverEntry* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    struct DriverEntry* self = NULL;

    try(kalloc_cleared(sizeof(DriverEntry), (void**) &self));
    ListNode_Init(&self->node);
    self->instance = driver;
    self->driverId = driverId;
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
    List_Init(&self->drivers);
    for (size_t i = 0; i < DRIVER_ID_HASH_CHAINS_COUNT; i++) {
        List_Init(&self->driversByDriverId[i]);
    }

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
        _DriverCatalog_DestroyDriverIdHashTable(self);
        _DriverCatalog_DestroyDriverTable(self);

        Lock_Deinit(&self->lock);
        kfree(self);
    }
}

static void _DriverCatalog_DestroyDriverTable(DriverCatalogRef _Nonnull self)
{
    ListNode* pCurEntry = self->drivers.first;

    while (pCurEntry != NULL) {
        ListNode* pNextEntry = pCurEntry->next;

        DriverEntry_Destroy((DriverEntry*)pCurEntry);
        pCurEntry = pNextEntry;
    }
        
    List_Deinit(&self->drivers);
}

static void _DriverCatalog_DestroyDriverIdHashTable(DriverCatalogRef _Nonnull self)
{
    for (size_t i = 0; i < DRIVER_ID_HASH_CHAINS_COUNT; i++) {
        ListNode* pCurEntry = self->driversByDriverId[i].first;
        
        while (pCurEntry != NULL) {
            ListNode* pNextEntry = pCurEntry->next;

            kfree(pCurEntry);
            pCurEntry = pNextEntry;
        }
    }
    
    for (size_t i = 0; i < DRIVER_ID_HASH_CHAINS_COUNT; i++) {
        List_Deinit(&self->driversByDriverId[i]);
    }
}

static DriverEntry* _Nullable _DriverCatalog_GetEntryByName(DriverCatalogRef _Locked _Nonnull self, const char* _Nonnull name)
{
    List_ForEach(&self->drivers, DriverEntry, {
        if (String_Equals(pCurNode->name, name)) {
            return pCurNode;
        }
    })

    return NULL;
}

static DriverIdHashEntry* _Nullable _DriverCatalog_GetHashIdEntryByDriverId(DriverCatalogRef _Locked _Nonnull self, DriverId driverId)
{
    List* chain = &self->driversByDriverId[driver_id_hash(driverId)];

    List_ForEach(chain,  DriverIdHashEntry, {
        if (pCurNode->entry->driverId == driverId) {
            return pCurNode;
        }
    })

    return NULL;
}

errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverId driverId, DriverRef _Consuming _Nonnull driver)
{
    decl_try_err();
    DriverEntry* entry = NULL;
    DriverIdHashEntry* driverIdEntry = NULL;

    Lock_Lock(&self->lock);
    err = DriverEntry_Create(name, driverId, driver, &entry);
    if (err == EOK) {
        err = kalloc_cleared(sizeof(DriverIdHashEntry), (void**)&driverIdEntry);
        if (err == EOK) {
            driverIdEntry->entry = entry;
            List_InsertBeforeFirst(&self->driversByDriverId[driver_id_hash(driverId)], &driverIdEntry->node);
            List_InsertBeforeFirst(&self->drivers, &entry->node);
        }
    }
    Lock_Unlock(&self->lock);

    return err;
}

void DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverId driverId)
{
    Lock_Lock(&self->lock);
    DriverIdHashEntry* hashEntry = _DriverCatalog_GetHashIdEntryByDriverId(self, driverId);

    if (hashEntry) {
        List_Remove(&self->driversByDriverId[driver_id_hash(driverId)], &hashEntry->node);
        List_Remove(&self->drivers, &hashEntry->entry->node);
    }
    Lock_Unlock(&self->lock);
}

DriverId DriverCatalog_GetDriverIdForName(DriverCatalogRef _Nonnull self, const char* _Nonnull name)
{
    Lock_Lock(&self->lock);
    DriverEntry* entry = _DriverCatalog_GetEntryByName(self, name);
    DriverId driverId = (entry) ? entry->driverId : kDriverId_None;
    Lock_Unlock(&self->lock);

    return driverId;
}

DriverRef DriverCatalog_CopyDriverForName(DriverCatalogRef _Nonnull self, const char* _Nonnull name)
{
    Lock_Lock(&self->lock);
    DriverEntry* entry = _DriverCatalog_GetEntryByName(self, name);
    DriverRef driver = (entry) ? Object_RetainAs(entry->instance, Driver) : NULL;
    Lock_Unlock(&self->lock);

    return driver;
}

DriverRef DriverCatalog_CopyDriverForDriverId(DriverCatalogRef _Nonnull self, DriverId driverId)
{
    Lock_Lock(&self->lock);
    DriverIdHashEntry* hashEntry = _DriverCatalog_GetHashIdEntryByDriverId(self, driverId);
    DriverRef driver = (hashEntry) ? Object_RetainAs(hashEntry->entry->instance, Driver) : NULL;
    Lock_Unlock(&self->lock);

    return driver;
}


// Generates a new and unique driver ID that should be used to publish a driver.
DriverId GetNewDriverId(void)
{
    static volatile AtomicInt gNextAvailableId = 1;
    return (DriverId) AtomicInt_Increment(&gNextAvailableId);
}
