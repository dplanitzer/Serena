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
#define kRootInodeId 1


//
// RamFS Directories
//

typedef struct _RamFS_DirectoryEntry {
    InodeId     id;
    Character   filename[kMaxFilenameLength];
} RamFS_DirectoryEntry;


// Directory content organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
typedef struct _RamFS_Directory {
    struct _GenericArray    header;
    InodeId                 inid;
    Int                     linkCount;
    UserId                  uid;
    GroupId                 gid;
    FilePermissions         permissions;
    FileOffset              size;
} RamFS_Directory;
typedef RamFS_Directory* RamFS_DirectoryRef;

static void RamFS_Directory_Destroy(RamFS_DirectoryRef _Nullable self);
static ErrorCode RamFS_Directory_AddWellKnownEntry(RamFS_DirectoryRef _Nonnull self, const Character* _Nonnull pName, InodeId id);


//
// RamFS
//

CLASS_IVARS(RamFS, Filesystem,
    Lock                lock;           // Shared between filesystem proper and inodes
    User                rootDirUser;    // User we should use for the root directory
    ConditionVariable   notifier;
    PointerArray        dnodes;
    Int                 nextAvailableInodeId;
    Bool                isMounted;
    Bool                isReadOnly;     // true if mounted read-only; false if mounted read-write
);

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self);
static ErrorCode RamFS_FormatWithEmptyFilesystem(RamFSRef _Nonnull self);

#endif /* RamFSPriv_h */
