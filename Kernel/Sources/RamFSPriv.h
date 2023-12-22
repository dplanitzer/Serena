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

typedef struct _RamDirectoryEntry {
    InodeId     id;
    Character   filename[kMaxFilenameLength];
} RamDirectoryEntry;


// Directory content organisation:
// [0] "."
// [1] ".."
// [2] userEntry0
// .
// [n] userEntryN-1
typedef struct _GenericArray RamDirectoryContent;
typedef RamDirectoryContent* RamDirectoryContentRef;


typedef struct _RamDiskNode {
    InodeId                             inid;
    UserId                              uid;
    GroupId                             gid;
    FilePermissions                     permissions;
    Int                                 linkCount;
    InodeType                           type;
    FileOffset                          size;
    RamDirectoryContentRef _Nullable    content;
} RamDiskNode;
typedef RamDiskNode* RamDiskNodeRef;


enum RamDirectoryQueryKind {
    kDirectoryQuery_PathComponent,
    kDirectoryQuery_InodeId
};

typedef struct RamDirectoryQuery {
    Int     kind;
    union _u {
        const PathComponent* _Nonnull   pc;
        InodeId                         id;
    }       u;
} RamDirectoryQuery;

static ErrorCode RamDiskNode_Create(InodeId id, InodeType type, RamDiskNodeRef _Nullable * _Nonnull pOutNode);
static void RamDiskNode_Destroy(RamDiskNodeRef _Nullable self);


//
// RamFS
//

CLASS_IVARS(RamFS, Filesystem,
    Lock                lock;           // Shared between filesystem proper and inodes
    User                rootDirUser;    // User we should use for the root directory
    ConditionVariable   notifier;
    InodeId             rootDirId;
    PointerArray        dnodes;         // Array<RamDiskNodeRef>
    Int                 nextAvailableInodeId;
    Bool                isMounted;
    Bool                isReadOnly;     // true if mounted read-only; false if mounted read-write
);

static InodeId RamFS_GetNextAvailableInodeId_Locked(RamFSRef _Nonnull self);
static ErrorCode RamFS_FormatWithEmptyFilesystem(RamFSRef _Nonnull self);
static ErrorCode RamFS_CreateDirectoryDiskNode(RamFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId);

#endif /* RamFSPriv_h */
