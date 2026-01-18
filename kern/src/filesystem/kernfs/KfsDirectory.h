//
//  KfsDirectory.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsDirectory_h
#define KfsDirectory_h

#include "KfsNode.h"
#include <ext/queue.h>
#include <filesystem/PathComponent.h>


#define MAX_LINK_COUNT      65535
#define MAX_NAME_LENGTH     10


// A directory entry
typedef struct KfsDirectoryEntry {
    deque_node_t    sibling;
    ino_t           inid;
    int8_t          nameLength;
    char            name[MAX_NAME_LENGTH];
} KfsDirectoryEntry;


open_class(KfsDirectory, KfsNode,
    deque_t/*<KfsDirectoryEntry>*/  entries;
);
open_class_funcs(KfsDirectory, KfsNode,
);


extern errno_t KfsDirectory_Create(KernFSRef _Nonnull fs, ino_t inid, mode_t permissions, uid_t uid, gid_t gid, ino_t pnid, KfsNodeRef _Nullable * _Nonnull pOutSelf);

extern bool KfsDirectory_IsEmpty(KfsDirectoryRef _Nonnull _Locked self);
extern errno_t _Nullable KfsDirectory_GetEntryForName(KfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull pc, KfsDirectoryEntry* _Nullable * _Nonnull pOutEntry);
extern errno_t KfsDirectory_GetNameOfEntryWithId(KfsDirectoryRef _Nonnull _Locked self, ino_t inid, MutablePathComponent* _Nonnull mpc);

extern errno_t KfsDirectory_CanAcceptEntry(KfsDirectoryRef _Nonnull _Locked self, const PathComponent* _Nonnull name, mode_t fileType);
extern errno_t KfsDirectory_InsertEntry(KfsDirectoryRef _Nonnull _Locked self, ino_t inid, bool isChildDir, const PathComponent* _Nonnull pc);
extern errno_t KfsDirectory_RemoveEntry(KfsDirectoryRef _Nonnull _Locked self, InodeRef _Nonnull pNodeToRemove);

#endif /* KfsDirectory_h */
