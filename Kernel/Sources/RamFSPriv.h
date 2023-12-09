//
//  RamFSPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 12/7/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef RamFSPriv_h
#define RamFSPriv_h

#include "RamFS.h"
#include "ConditionVariable.h"
#include "Lock.h"

#define kMaxFilenameLength  32


//
// RamFS Directories
//

typedef struct _GenericArray RamFS_DirectoryHeader;

typedef struct _RamFS_DirectoryEntry {
    InodeRef _Nonnull   node;
    Character           filename[kMaxFilenameLength];
} RamFS_DirectoryEntry;


OPEN_CLASS_WITH_REF(RamFS_Directory, Inode,
    // [0] "."
    // [1] ".."
    // [2] userEntry0
    // .
    // [n] userEntryN-1
    RamFS_DirectoryHeader       header;
);
typedef struct _RamFS_DirectoryMethodTable {
    InodeMethodTable    super;
} RamFS_DirectoryMethodTable;

static ErrorCode DirectoryNode_AddWellKnownEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pName, InodeRef _Nonnull pNode);


//
// RamFS
//

CLASS_IVARS(RamFS, Filesystem,
    Lock                            lock;           // Shared between filesystem proper and inodes
    User                            rootDirUser;    // User we should use for the root directory
    RamFS_DirectoryRef _Nullable    root;           // NULL when not mounted; root node while mounted
    ConditionVariable               notifier;
    Int                             nextAvailableInodeId;
    Int                             busyCount;      // == 0 -> not busy; > 0 -> busy
    Bool                            isReadOnly;     // true if mounted read-only; false if mounted read-write
);

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self);

#endif /* RamFSPriv_h */
