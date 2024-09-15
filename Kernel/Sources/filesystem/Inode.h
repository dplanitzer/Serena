//
//  Inode.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Inode_h
#define Inode_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <dispatcher/Lock.h>
#include <System/Directory.h>
#include <System/File.h>

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
typedef struct Inode {
    TimeInterval                    accessTime;
    TimeInterval                    modificationTime;
    TimeInterval                    statusChangeTime;
    FileOffset                      size;       // File size
    Lock                            lock;
    FilesystemRef _Weak _Nonnull    filesystem; // The owning filesystem instance
    InodeId                         inid;       // Filesystem specific ID of the inode
    int                             useCount;   // Number of entities that are using this inode at this moment. Incremented on acquisition and decremented on relinquishing (protected by the FS inode management lock)
    int                             linkCount;  // Number of directory entries referencing this inode. Incremented on create/link and decremented on unlink
    void*                           refcon;     // Filesystem specific information
    FileType                        type;
    uint8_t                         flags;
    FilePermissions                 permissions;
    UserId                          uid;
    GroupId                         gid;
} Inode;


//
// The following functions may be called without holding the inode lock.
//

// Reacquiring and relinquishing an existing inode

#define Inode_Reacquire(__self) \
    Filesystem_ReacquireNode((__self)->filesystem, __self)

#define Inode_Relinquish(__self) \
    Filesystem_RelinquishNode((__self)->filesystem, __self)

extern void Inode_UnlockRelinquish(InodeRef _Nullable _Locked self);


//
// Locking/unlocking an inode
//

#define Inode_Lock(__self) \
    Lock_Lock(&((InodeRef)__self)->lock)

#define Inode_Unlock(__self) \
    Lock_Unlock(&((InodeRef)__self)->lock)


//
// The caller must hold the inode lock while calling any of the functions below.
//

// Inode timestamps

#define Inode_GetAccessTime(__self) \
    (__self)->accessTime

#define Inode_SetAccessTime(__self, __time) \
    (__self)->accessTime = __time

#define Inode_GetModificationTime(__self) \
    (__self)->modificationTime

#define Inode_SetModificationTime(__self, __time) \
    (__self)->modificationTime = __time

#define Inode_GetStatusChangeTime(__self) \
    (__self)->statusChangeTime

#define Inode_SetStatusChangeTime(__self, __time) \
    (__self)->statusChangeTime = __time


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

void Inode_Unlink(InodeRef _Nonnull self);


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



// Inode file size

#define Inode_GetFileSize(__self) \
    (__self)->size

#define Inode_SetFileSize(__self, __size) \
    (__self)->size = __size

#define Inode_IncrementFileSize(__self, __delta) \
    (__self)->size += (__delta)

#define Inode_DecrementFileSize(__self, __delta) \
    (__self)->size -= (__delta)



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
    Filesystem_GetId((__self)->filesystem)

// Returns the filesystem that owns the inode. The returned reference is unowned
// and guaranteed to be valid as long as the inode reference remains valid.
#define Inode_GetFilesystem(__self) \
    (__self)->filesystem

// Returns true if the receiver and 'pOther' are the same node; false otherwise
extern bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther);


//
// Only filesystem implementations should call the following functions.
//

// Creates an instance an Inode.
extern errno_t Inode_Create(FilesystemRef _Nonnull pFS, InodeId id,
                    FileType type,
                    int linkCount,
                    UserId uid, GroupId gid, FilePermissions permissions,
                    FileOffset size,
                    TimeInterval accessTime, TimeInterval modTime, TimeInterval statusChangeTime,
                    void* refcon,
                    InodeRef _Nullable * _Nonnull pOutNode);
extern void Inode_Destroy(InodeRef _Nonnull self);

#endif /* Inode_h */
