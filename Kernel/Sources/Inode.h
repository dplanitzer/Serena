//
//  Inode.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Inode_h
#define Inode_h

#include <klib/klib.h>
#include <System/File.h>
#include "Lock.h"

CLASS_FORWARD(Filesystem);

// Inode flags
enum {
    kInodeFlag_IsMountpoint = 0x01,     // [FilesystemManager lock]
    kInodeFlag_Accessed = 0x04,         // [Inode lock] access date needs update
    kInodeFlag_Updated = 0x02,          // [Inode lock] mod date needs update
    kInodeFlag_StatusChanged = 0x08,    // [Inode lock] status changed date needs update
};


// An Inode represents the meta information of a file or directory. This is an
// abstract class that must be subclassed and fully implemented by a file system.
// See the description of the Filesystem class to learn about how locking for
// Inodes works.
typedef struct _Inode {
    TimeInterval        accessTime;
    TimeInterval        modificationTime;
    TimeInterval        statusChangeTime;
    FileOffset          size;       // File size
    Lock                lock;
    FilesystemId        fsid;       // Globally unique ID of the filesystem that owns this node
    InodeId             inid;       // Filesystem specific ID of the inode
    int                 useCount;   // Number of entities that are using this inode at this moment. Incremented on acquisition and decremented on relinquishing (protected by the FS inode management lock)
    int                 linkCount;  // Number of directory entries referencing this inode. Incremented on create/link and decremented on unlink
    void*               refcon;     // Filesystem specific information
    FileType            type;
    uint8_t               flags;
    FilePermissions     permissions;
    UserId              uid;
    GroupId             gid;
} Inode;
typedef struct _Inode* InodeRef;


//
// Only filesystem implementations should call the following functions.
//

// Creates an instance of the abstract Inode class. Should only ever be called
// by the implement of a creation function for a concrete Inode subclass.
extern errno_t Inode_Create(FilesystemId fsid, InodeId id,
                    FileType type,
                    int linkCount,
                    UserId uid, GroupId gid, FilePermissions permissions,
                    FileOffset size,
                    TimeInterval accessTime, TimeInterval modTime, TimeInterval statusChangeTime,
                    void* refcon,
                    InodeRef _Nullable * _Nonnull pOutNode);
extern void Inode_Destroy(InodeRef _Nonnull self);

// Inode locking

#define Inode_Lock(__self) \
    Lock_Lock(&((InodeRef)__self)->lock)

#define Inode_Unlock(__self) \
    Lock_Unlock(&((InodeRef)__self)->lock)


// Inode timestamps

#define Inode_GetAccessTime(__self) \
    (__self)->accessTime

#define Inode_GetModificationTime(__self) \
    (__self)->modificationTime

#define Inode_GetStatusChangeTime(__self) \
    (__self)->statusChangeTime


// Inode modified and timestamp changed flags

#define Inode_IsModified(__self) \
    (((__self)->flags & (kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged)) != 0)

#define Inode_SetModified(__self, __mflags) \
    ((__self)->flags |= ((__mflags) & (kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged)))

#define Inode_ClearModified(__self) \
    ((__self)->flags &= ~(kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged))

#define Inode_IsAccessed(__self) \
    (((__self)->flags & kInodeFlag_Accessed) != 0)

#define Inode_IsUpdated(__self) \
    (((__self)->flags & kInodeFlag_Updated) != 0)

#define Inode_IsStatusChanged(__self) \
    (((__self)->flags & kInodeFlag_StatusChanged) != 0)


// Inode link counts

#define Inode_GetLinkCount(__self) \
    (__self)->linkCount

#define Inode_Link(__self) \
    (__self)->linkCount++

#define Inode_Unlink(__self) \
    (__self)->linkCount--


// Associate/disassociate filesystem specific information with the node. The
// inode will not free this pointer.
#define Inode_GetRefConAs(__self, __type) \
    (__type)((__self)->refcon)

#define Inode_SetRefCon(__self, __ptr) \
    (__self)->refcon = (__ptr)


// Returns the permissions of the node.
#define Inode_GetFilePermissions(__self) \
    (__self)->permissions

#define Inode_SetFilePermissions(__self, __perms) \
    (__self)->permissions = __perms

// Returns the User ID of the node.
#define Inode_GetUserId(__self) \
    (__self)->uid

#define Inode_SetUserId(__self, __uid) \
    (__self)->uid = __uid

// Returns the group ID of the node.
#define Inode_GetGroupId(__self) \
    (__self)->gid

#define Inode_SetGroupId(__self, __gid) \
    (__self)->gid = __gid

// Returns EOK if the given user has at least the permissions 'permission' to
// access and/or manipulate the node; a suitable error code otherwise. The
// 'permission' parameter represents a set of the permissions of a single
// permission scope.
extern errno_t Inode_CheckAccess(InodeRef _Nonnull self, User user, FilePermissions permission);


// Inode file size

#define Inode_GetFileSize(__self) \
    (__self)->size

#define Inode_SetFileSize(__self, __size) \
    (__self)->size = __size

#define Inode_IncrementFileSize(__self, __delta) \
    (__self)->size += (__delta)

#define Inode_DecrementFileSize(__self, __delta) \
    (__self)->size -= (__delta)


// Returns a file info record from the node data.
extern void Inode_GetFileInfo(InodeRef _Nonnull self, FileInfo* _Nonnull pOutInfo);

// Modifies the node's file info if the operation is permissible based on the
// given user and inode permissions status.
extern errno_t Inode_SetFileInfo(InodeRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);


//
// Only the FilesystemManager should call the following functions. The filesystem
// manager must hold the inode lock while calling these functions.
//

#define Inode_IsMountpoint(__self) \
    ((__self)->flags & kInodeFlag_IsMountpoint) != 0

#define Inode_SetMountpoint(__self, __flag) \
    if(__flag) {(__self)->flags |= kInodeFlag_IsMountpoint;} else {(__self)->flags &= ~kInodeFlag_IsMountpoint;}

    
//
// The following functions may be used by filesystem implementations and code
// that lives outside of a filesystem. The caller does not have to hold the inode
// lock to use these functions.
//

// Returns the type of the node.
#define Inode_GetFileType(__self) \
    (__self)->type

// Returns true if the node is a directory; false otherwise.
#define Inode_IsDirectory(__self) \
    (Inode_GetFileType(__self) == kFileType_Directory)

// Returns true if the node is a regular file; false otherwise.
#define Inode_IsRegularFile(__self) \
    (Inode_GetFileType(__self) == kFileType_RegularFile)

// Returns the filesystem specific ID of the node.
#define Inode_GetId(__self) \
    (__self)->inid

// Returns the ID of the filesystem to which this node belongs.
#define Inode_GetFilesystemId(__self) \
    (__self)->fsid

// Returns a strong reference to the filesystem that owns the given note. Returns
// NULL if the filesystem isn't mounted.
extern FilesystemRef Inode_CopyFilesystem(InodeRef _Nonnull self);

// Returns true if the receiver and 'pOther' are the same node; false otherwise
extern bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther);

// Reacquires the given node and returns a new reference to the node. The node
// is returned in locked state.
extern InodeRef _Nonnull _Locked Inode_Reacquire(InodeRef _Nonnull self);

// Reacquires the given node and returns a new reference to the node. The node
// is returned in unlocked state.
extern InodeRef _Nonnull Inode_ReacquireUnlocked(InodeRef _Nonnull self);

// Relinquishes the node.
extern void Inode_Relinquish(InodeRef _Nonnull self);

#endif /* Inode_h */
