//
//  Inode.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/07/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Inode_h
#define Inode_h

#include <kobj/Any.h>
#include <klib/List.h>
#include <dispatcher/Lock.h>
#include <System/Directory.h>
#include <System/File.h>

// Inode flags
enum {
    kInodeFlag_Accessed = 0x04,         // [Inode lock] access date needs update
    kInodeFlag_Updated = 0x02,          // [Inode lock] mod date needs update
    kInodeFlag_StatusChanged = 0x08,    // [Inode lock] status changed date needs update
};


// Inode state
enum {
    kInodeState_Reading = 0,
    kInodeState_Cached,
    kInodeState_Writeback,
    kInodeState_Deleting
};


// An Inode represents the meta information of a file or directory. This is an
// abstract class that must be subclassed and fully implemented by a file system.
// See the description of the Filesystem class to learn about how locking for
// Inodes works.
open_class(Inode, Any,
    ListNode                        sibling;        // Protected by Filesystem.inLock
    int                             useCount;       // Number of clients currently using this inode. Incremented on acquisition and decremented on relinquishing (protected by Filesystem.inLock)
    int                             state;

    Lock                            lock;
    TimeInterval                    accessTime;
    TimeInterval                    modificationTime;
    TimeInterval                    statusChangeTime;
    off_t                           size;       // File size
    FilesystemRef _Weak _Nonnull    filesystem; // The owning filesystem instance
    ino_t                           inid;       // Filesystem specific ID of the inode
    ino_t                           pnid;       // Filesystem specific ID of the parent inode (directory in which inid is stored)
    nlink_t                         linkCount;  // Number of directory entries referencing this inode. Incremented on create/link and decremented on unlink
    FileType                        type;
    uint8_t                         flags;
    FilePermissions                 permissions;
    uid_t                           uid;
    gid_t                           gid;
);
any_subclass_funcs(Inode,
    // Invoked when the last strong reference of the inode has been released.
    // Overrides should release all resources held by the inode.
    // Note that you do not need to call the super implementation from your
    // override. The object runtime takes care of that automatically. 
    void (*deinit)(void* _Nonnull self);


    //
    // I/O Channels
    //

    // Creates and returns an I/O channel that is suitable for reading/writing
    // data.
    errno_t (*createChannel)(void* _Nonnull _Locked self, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);


    //
    // Get/Set Inode Attributes
    //

    // Returns the file information of the given node. The node may be of any
    // type.
    // Override: Optional
    // Default Behavior: Returns the node's file info
    errno_t (*getInfo)(void* _Nonnull _Locked self, finfo_t* _Nonnull pOutInfo);

    // Modifies one or more attributes stored in the file information of the given
    // node. The node may be of any type.
    // Override: Optional
    // Default Behavior: Updates the inode's file info
    errno_t (*setInfo)(void* _Nonnull _Locked self, uid_t uid, gid_t gid, fmutinfo_t* _Nonnull pInfo);

    // Sets the user and group id of the inode owner.
    // Override: Optional
    // Default Behavior: Updates the inode's owner info
    errno_t (*setOwner)(void* _Nonnull _Locked self, uid_t uid, gid_t gid);


    //
    // File Specific Operations
    //

    // Reads up to 'nBytesToRead' bytes starting at the file offset 'pInOutOffset'
    // from the file 'pFile'.
    errno_t (*read)(void* _Nonnull _Locked self, IOChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

    // Writes up to 'nBytesToWrite' bytes starting at file offset 'pInOutOffset'
    // to the file 'pFile'.
    errno_t (*write)(void* _Nonnull _Locked self, IOChannelRef _Nonnull _Locked pChannel, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

    // Change the size of the file 'pFile' to 'length'. 'length' is guaranteed
    // to be >= 0. No longer needed blocks are deallocated if the new length is
    // less than the old length and zero-fille blocks are allocated and assigned
    // to the file if the new length is longer than the old length. Note that a
    // filesystem implementation is free to defer the actual allocation of the
    // new blocks until an attempt is made to read or write them.
    errno_t (*truncate)(void* _Nonnull _Locked self, off_t length);
);


//
// The following functions may be called without holding the inode lock.
//

// Reacquiring and relinquishing an existing inode

#define Inode_Reacquire(__self) \
    Filesystem_ReacquireNode(((InodeRef)__self)->filesystem, __self)

extern errno_t Inode_Relinquish(InodeRef _Nullable self);
extern errno_t Inode_UnlockRelinquish(InodeRef _Nullable _Locked self);


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
    ((InodeRef)__self)->accessTime

#define Inode_SetAccessTime(__self, __time) \
    ((InodeRef)__self)->accessTime = (__time)

#define Inode_GetModificationTime(__self) \
    ((InodeRef)__self)->modificationTime

#define Inode_SetModificationTime(__self, __time) \
    ((InodeRef)__self)->modificationTime = (__time)

#define Inode_GetStatusChangeTime(__self) \
    ((InodeRef)__self)->statusChangeTime

#define Inode_SetStatusChangeTime(__self, __time) \
    ((InodeRef)__self)->statusChangeTime = (__time)


// Inode modified and timestamp changed flags

#define Inode_IsModified(__self) \
    ((((InodeRef)__self)->flags & (kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged)) != 0)

#define Inode_SetModified(__self, __mflags) \
    (((InodeRef)__self)->flags |= ((__mflags) & (kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged)))

#define Inode_IsAccessed(__self) \
    ((((InodeRef)__self)->flags & kInodeFlag_Accessed) != 0)

#define Inode_IsUpdated(__self) \
    ((((InodeRef)__self)->flags & kInodeFlag_Updated) != 0)

#define Inode_IsStatusChanged(__self) \
    ((((InodeRef)__self)->flags & kInodeFlag_StatusChanged) != 0)


// Inode link counts

#define Inode_GetLinkCount(__self) \
    ((InodeRef)__self)->linkCount

#define Inode_Link(__self) \
    ((InodeRef)__self)->linkCount++

void Inode_Unlink(InodeRef _Nonnull self);


// Returns the permissions of the node.
#define Inode_GetFilePermissions(__self) \
    ((InodeRef)__self)->permissions

#define Inode_SetFilePermissions(__self, __perms) \
    ((InodeRef)__self)->permissions = (__perms)

// Returns the User ID of the node.
#define Inode_GetUserId(__self) \
    ((InodeRef)__self)->uid

#define Inode_SetUserId(__self, __uid) \
    ((InodeRef)__self)->uid = (__uid)

// Returns the group ID of the node.
#define Inode_GetGroupId(__self) \
    ((InodeRef)__self)->gid

#define Inode_SetGroupId(__self, __gid) \
    ((InodeRef)__self)->gid = (__gid)



// Inode file size

#define Inode_GetFileSize(__self) \
    ((InodeRef)__self)->size

#define Inode_SetFileSize(__self, __size) \
    ((InodeRef)__self)->size = (__size)

#define Inode_IncrementFileSize(__self, __delta) \
    ((InodeRef)__self)->size += (__delta)

#define Inode_DecrementFileSize(__self, __delta) \
    ((InodeRef)__self)->size -= (__delta)



//
// The following functions may be used by filesystem implementations and code
// that lives outside of a filesystem. The caller does not have to hold the inode
// lock to use these functions.
//

// Returns the type of the node.
#define Inode_GetFileType(__self) \
    ((InodeRef)__self)->type

// Returns true if the node is a directory; false otherwise.
#define Inode_IsDirectory(__self) \
    (Inode_GetFileType((InodeRef)__self) == S_IFDIR)

// Returns true if the node is a regular file; false otherwise.
#define Inode_IsRegularFile(__self) \
    (Inode_GetFileType((InodeRef)__self) == S_IFREG)


// Returns the inode id of the parent inode. This function may return 0 because
// tracking the parent node for the given inode type is not supported by the
// filesystem.
#define Inode_GetParentId(__self) \
((InodeRef)__self)->pnid

// Sets the inode id of the parent inode. This should only be called by the
// Filesystem.move() function
#define Inode_SetParentId(__self, __id) \
((InodeRef)__self)->pnid = (__id)


// Returns the filesystem specific ID of the node.
#define Inode_GetId(__self) \
    ((InodeRef)__self)->inid

// Returns the ID of the filesystem to which this node belongs.
#define Inode_GetFilesystemId(__self) \
    Filesystem_GetId(((InodeRef)__self)->filesystem)

// Returns the filesystem that owns the inode. The returned reference is unowned
// and guaranteed to be valid as long as the inode reference remains valid.
#define Inode_GetFilesystem(__self) \
    ((InodeRef)__self)->filesystem

#define Inode_GetFilesystemAs(__self, __class) \
    ((__class##Ref)((InodeRef)__self)->filesystem)


// Returns true if the receiver and 'pOther' are the same node; false otherwise
extern bool Inode_Equals(InodeRef _Nonnull self, InodeRef _Nonnull pOther);


#define Inode_CreateChannel(__self, __mode, __pOutChannel) \
invoke_n(createChannel, Inode, __self, __mode, __pOutChannel)


#define Inode_GetInfo(__self, __pOutInfo) \
invoke_n(getInfo, Inode, __self, __pOutInfo)

#define Inode_SetInfo(__self, __uid, __gid, __pInfo) \
invoke_n(setInfo, Inode, __self, __uid, __gid, __pInfo)

#define Inode_SetOwner(__self, __uid, __gid) \
invoke_n(setOwner, Inode, __self, __uid, __gid)


#define Inode_Read(__self, __pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead) \
invoke_n(read, Inode, __self, (IOChannelRef)__pChannel, __pBuffer, __nBytesToRead, __nOutBytesRead)

#define Inode_Write(__self, __pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten) \
invoke_n(write, Inode, __self, (IOChannelRef)__pChannel, __pBuffer, __nBytesToWrite, __nOutBytesWritten)

#define Inode_Truncate(__self, __length) \
invoke_n(truncate, Inode, __self, __length)


//
// Only filesystem implementations should call the following functions.
//

// Creates an instance of the inode subclass 'pClass'. 'id' is the unique id of
// the inode. This id must be unique with respect to the owning filesystem 'fs'.
// 'pnid' is the id of the parent inode. This is the directory inside of which
// 'id' exists. Note that the parent id is optional: if it is 0 then this means
// that the filesystem does not support tracking the parent for inodes of type
// 'type'. If it is > 0 then the provided id is the id of the directory in which
// the inode of type 'type' and 'id' resides. Note that the parent id of the root
// node of the filesystem should be equal to the root node id.
extern errno_t Inode_Create(Class* _Nonnull pClass,
                    FilesystemRef _Nonnull fs, ino_t id,
                    FileType type, int linkCount,
                    uid_t uid, gid_t gid, FilePermissions permissions,
                    off_t size,
                    TimeInterval accessTime, TimeInterval modTime, TimeInterval statusChangeTime,
                    ino_t pnid,
                    InodeRef _Nullable * _Nonnull pOutNode);
extern void Inode_Destroy(InodeRef _Nonnull self);

// Unconditionally writes the inode's metadata to disk. Does not write the file
// content.
extern errno_t Inode_Writeback(InodeRef _Nonnull _Locked self);

#endif /* Inode_h */
