//
//  DevFS_Catalog.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DevFSPriv.h"


//
// MARK: -
// DfsItem
//

static void DfsItem_Init(DfsItem* _Nonnull self, InodeId inid, FileType type, FilePermissions permissions, UserId uid, GroupId gid)
{
    self->modificationTime = FSGetCurrentTime();
    self->accessTime = self->modificationTime;
    self->statusChangeTime = self->modificationTime;
    self->size = 0;
    self->inid = inid;
    self->linkCount = 1;
    self->type = type;
    self->permissions = permissions;
    self->uid = uid;
    self->gid = gid;
}

void DfsItem_Destroy(DfsItem* _Nullable self)
{
    if (self) {
        switch (self->type) {
            case kFileType_Directory:
                DfsDirectoryItem_Destroy((DfsDirectoryItem*)self);
                break;

            case kFileType_Device:
                DfsDriverItem_Destroy((DfsDriverItem*)self);
                break;

            default:
                abort();
                break;
        }
    }
}


//
// MARK: -
// DirectoryItem
//

errno_t DfsDirectoryItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, InodeId pnid, DfsDirectoryItem* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DfsDirectoryItem* self = NULL;
    PathComponent pc;

    try(FSAllocateCleared(sizeof(DfsDirectoryItem), (void**)&self));
    DfsItem_Init(&self->super, inid, kFileType_Directory, permissions, uid, gid);
    
    try(DfsDirectoryItem_AddEntry(self, inid, &kPathComponent_Self));
    try(DfsDirectoryItem_AddEntry(self, pnid, &kPathComponent_Parent));
    self->super.size = 2 * sizeof(DfsDirectoryEntry);

    *pOutSelf = self;
    return EOK;

catch:
    DfsDirectoryItem_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DfsDirectoryItem_Destroy(DfsDirectoryItem* _Nullable self)
{
    if (self) {
        List_ForEach(&self->entries, DirectoryEntry,
            FSDeallocate(pCurNode);
        )
        FSDeallocate(self);
    }
}

bool DfsDirectoryItem_IsEmpty(DfsDirectoryItem* _Nonnull self)
{
    return List_IsEmpty(&self->entries);
}

errno_t _Nullable DfsDirectoryItem_GetEntryForName(DfsDirectoryItem* _Nonnull self, const PathComponent* _Nonnull pc, DfsDirectoryEntry* _Nullable * _Nonnull pOutEntry)
{
    *pOutEntry = NULL;

    if (pc->count == 0) {
        return ENOENT;
    }
    else if (pc->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (PathComponent_EqualsString(pc, pCurNode->name, pCurNode->nameLength)) {
            *pOutEntry = pCurNode;
            return EOK;
        }
    )

    return ENOENT;
}

errno_t DfsDirectoryItem_GetNameOfEntryWithId(DfsDirectoryItem* _Nonnull self, InodeId inid, MutablePathComponent* _Nonnull mpc)
{
    DfsDirectoryEntry* entry = NULL;

    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (pCurNode->inid == inid) {
            entry = pCurNode;
            break;
        }
    )

    if (entry == NULL) {
        mpc->count = 0;
        return ENOENT;
    }
    else if (entry->nameLength > mpc->capacity) {
        mpc->count = 0;
        return ERANGE;
    }
    else {
        mpc->count = entry->nameLength;
        memcpy(mpc->name, entry->name, entry->nameLength);
        return EOK;
    }
}

errno_t DfsDirectoryItem_AddEntry(DfsDirectoryItem* _Nonnull self, InodeId inid, const PathComponent* _Nonnull pc)
{
    decl_try_err();
    DfsDirectoryEntry* entry;

    if (pc->count > MAX_NAME_LENGTH) {
        return ENAMETOOLONG;
    }

    if ((err = FSAllocate(sizeof(DfsDirectoryEntry), (void**)&entry)) == EOK) {
        entry->sibling.next = NULL;
        entry->sibling.prev = NULL;
        entry->inid = inid;
        entry->nameLength = pc->count;
        memcpy(entry->name, pc->name, pc->count);

        List_InsertAfterLast(&self->entries, &entry->sibling);
    }

    return err;
}

errno_t DfsDirectoryItem_RemoveEntry(DfsDirectoryItem* _Nonnull self, InodeId inid)
{
    DfsDirectoryEntry* entry = NULL;

    List_ForEach(&self->entries, DfsDirectoryEntry,
        if (pCurNode->inid == inid) {
            entry = pCurNode;
            break;
        }
    )

    if (entry == NULL) {
        return ENOENT;
    }

    List_Remove(&self->entries, &entry->sibling);
    FSDeallocate(entry);
}


//
// MARK: -
// DriverItem
//

errno_t DfsDriverItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, DriverRef _Nonnull pDriver, intptr_t arg, DfsDriverItem* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DfsDriverItem* self = NULL;

    try(FSAllocateCleared(sizeof(DfsDriverItem), (void**)&self));
    DfsItem_Init(&self->super, inid, kFileType_Device, permissions, uid, gid);
    self->super.size = sizeof(DfsDriverItem);
    self->instance = Object_RetainAs(pDriver, Driver);
    self->arg = arg;

catch:
    *pOutSelf = self;
    return err;
}

void DfsDriverItem_Destroy(DfsDriverItem* _Nullable self)
{
    if (self) {
        Object_Release(self->instance);
        self->instance = NULL;
        FSDeallocate(self);
    }
}
