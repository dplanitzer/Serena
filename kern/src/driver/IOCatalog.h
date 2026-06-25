//
//  IOCatalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef IOCatalog_h
#define IOCatalog_h

#include <kobj/Object.h>
#include <kpi/fs_perms.h>
#include <kpi/ioctl.h>
#include <kpi/types.h>
#include <filemanager/ResolvedPath.h>


typedef errno_t (*IOCreateHandlerFunc)(InodeRef _Nonnull ip, fd_flags_t flags, HandlerRef _Nullable * _Nullable pOutHandler);


typedef struct DirEntry {
    const char* _Nonnull    name;
    uid_t                   uid;
    gid_t                   gid;
    fs_perms_t              perms;
} DirEntry;


typedef struct DriverEntry {
    const char* _Nonnull            name;
    IOCreateHandlerFunc _Nonnull    func;
    uid_t                           uid;
    gid_t                           gid;
    fs_perms_t                      perms;
} DriverEntry;


typedef struct CatalogEntry {
    const char* _Nonnull            name;
    ObjectRef _Nullable             resource;
    IOCreateHandlerFunc _Nonnull    func;
    uid_t                           uid;
    gid_t                           gid;
    fs_perms_t                      perms;
} CatalogEntry;


// The non-persistent, globally unique ID of a published catalog entry.
// This ID does not survive a system reboot. Id 0 represents a catalog
// entry that does not exist.
typedef uint32_t    CatalogId;

// Means no catalog entry
#define kCatalogId_None   0


extern IOCatalogRef gIOCatalog;


extern errno_t IOCatalog_Create(IOCatalogRef _Nullable * _Nonnull pOutSelf);


extern FilesystemRef _Nonnull IOCatalog_GetFilesystem(IOCatalogRef _Nonnull self);

// Looks up the inode for the given path and returns it.
extern errno_t IOCatalog_AcquireNodeForPath(IOCatalogRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp);

// Returns the inode that represents the given driver. A suitable error is
// returned if the driver has no inode associated with it, etc. You must call
// Inode_Relinquish() when you no longer need the inode.
extern errno_t IOCatalog_AcquireNodeForDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver, InodeRef _Nullable * _Nonnull pOutNode);

// Opens the catalog entry at the in-kernel path 'path' with mode 'mode' and
// returns the resulting channel in 'pOutHandler'. This call does not support
// opening a folder.
extern errno_t IOCatalog_Open(IOCatalogRef _Nonnull self, const char* _Nonnull path, fd_flags_t oflags, HandlerRef _Nullable * _Nonnull pOutHandler);


// Removes a published entry from the catalog.
extern errno_t IOCatalog_Unpublish(IOCatalogRef _Nonnull self, CatalogId entryId);


// Publish the driver instance 'drv' with the name 'name' in the driver catalog.
// Returns a suitable error if another entry with the same name already exists.
extern errno_t IOCatalog_PublishDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull drv, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId);

// Publishes a new special file entry in the catalog. The entry may be associated
// with an already existing resource 'resource'. The function 'func' is called
// when a handler for the resource is needed. 'resource' may be NULL.
extern errno_t IOCatalog_PublishEntry(IOCatalogRef _Nonnull self, const CatalogEntry* _Nonnull ce, did_t* _Nullable pOutId);

#endif /* IOCatalog_h */
