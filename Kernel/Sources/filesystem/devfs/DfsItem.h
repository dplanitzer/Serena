//
//  DfsItem.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsItem_h
#define DfsItem_h

#include <filesystem/PathComponent.h>
#include <klib/Error.h>
#include <klib/Types.h>
#include <klib/List.h>
#include <kobj/AnyRefs.h>
#include <System/TimeInterval.h>


#define MAX_LINK_COUNT      65535
#define MAX_NAME_LENGTH     10

typedef struct DfsItem {
    ListNode            inidChain;      // Inid hash chain link 
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    off_t               size;
    InodeId             inid;
    int                 linkCount;
    FileType            type;
    uint8_t             flags;
    FilePermissions     permissions;
    UserId              uid;
    GroupId             gid;
} DfsItem;

// A directory entry
typedef struct DfsDirectoryEntry {
    ListNode    sibling;
    InodeId     inid;
    int8_t      nameLength;
    char        name[MAX_NAME_LENGTH];
} DfsDirectoryEntry;

// A directory of drivers and child directories
typedef struct DfsDirectoryItem {
    DfsItem                     super;
    List/*<DfsDirectoryEntry>*/ entries;
} DfsDirectoryItem;

// A device entry
typedef struct DfsDeviceItem {
    DfsItem             super;
    DriverRef _Nonnull  instance;
    intptr_t            arg;
} DfsDeviceItem;


extern void DfsItem_Destroy(DfsItem* _Nullable self);

extern errno_t DfsDirectoryItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, InodeId pnid, DfsDirectoryItem* _Nullable * _Nonnull pOutSelf);
extern void DfsDirectoryItem_Destroy(DfsDirectoryItem* _Nullable self);
extern bool DfsDirectoryItem_IsEmpty(DfsDirectoryItem* _Nonnull self);
extern errno_t _Nullable DfsDirectoryItem_GetEntryForName(DfsDirectoryItem* _Nonnull self, const PathComponent* _Nonnull pc, DfsDirectoryEntry* _Nullable * _Nonnull pOutEntry);
extern errno_t DfsDirectoryItem_GetNameOfEntryWithId(DfsDirectoryItem* _Nonnull self, InodeId inid, MutablePathComponent* _Nonnull mpc);
extern errno_t DfsDirectoryItem_AddEntry(DfsDirectoryItem* _Nonnull self, InodeId inid, const PathComponent* _Nonnull pc);
extern errno_t DfsDirectoryItem_RemoveEntry(DfsDirectoryItem* _Nonnull self, InodeId inid);

extern errno_t DfsDeviceItem_Create(InodeId inid, FilePermissions permissions, UserId uid, GroupId gid, DriverRef _Nonnull pDriver, intptr_t arg, DfsDeviceItem* _Nullable * _Nonnull pOutSelf);
extern void DfsDeviceItem_Destroy(DfsDeviceItem* _Nullable self);

#endif /* DfsItem_h */
