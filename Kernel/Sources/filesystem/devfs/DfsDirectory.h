//
//  DfsDirectory.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsDirectory_h
#define DfsDirectory_h

#include "DfsNode.h"
#include <filesystem/PathComponent.h>
#include <klib/List.h>


#define MAX_LINK_COUNT      65535
#define MAX_NAME_LENGTH     10


// A directory entry
typedef struct DfsDirectoryEntry {
    ListNode    sibling;
    ino_t       inid;
    int8_t      nameLength;
    char        name[MAX_NAME_LENGTH];
} DfsDirectoryEntry;


open_class(DfsDirectory, DfsNode,
    List/*<DfsDirectoryEntry>*/ entries;
);
open_class_funcs(DfsDirectory, DfsNode,
);


extern errno_t DfsDirectory_Create(DevFSRef _Nonnull fs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, DfsNodeRef _Nullable * _Nonnull pOutSelf);

extern bool DfsDirectory_IsEmpty(DfsDirectoryRef _Nonnull _Locked self);
extern errno_t _Nullable DfsDirectory_GetEntryForName(DfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull pc, DfsDirectoryEntry* _Nullable * _Nonnull pOutEntry);
extern errno_t DfsDirectory_GetNameOfEntryWithId(DfsDirectoryRef _Nonnull _Locked self, ino_t inid, MutablePathComponent* _Nonnull mpc);

extern errno_t DfsDirectory_InsertEntry(DfsDirectoryRef _Nonnull _Locked self, ino_t inid, const PathComponent* _Nonnull pc);
extern errno_t DfsDirectory_RemoveEntry(DfsDirectoryRef _Nonnull _Locked self, ino_t inid);

#endif /* DfsDirectory_h */
