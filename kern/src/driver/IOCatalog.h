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

// Returns the inode that represents the given driver. A suitable error is
// returned if the driver has no inode associated with it, etc. You must call
// Inode_Relinquish() when you no longer need the inode.
extern errno_t IOCatalog_AcquireNodeForDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull driver, InodeRef _Nullable * _Nonnull pOutNode);


// Removes a published entry from the catalog.
extern errno_t IOCatalog_Unpublish(IOCatalogRef _Nonnull self, CatalogId entryId);


// Publishes a new special file entry in the catalog. The entry may be associated
// with an already existing resource 'resource'. The function 'func' is called
// when a handler for the resource is needed. 'resource' may be NULL.
extern errno_t IOCatalog_PublishEntry(IOCatalogRef _Nonnull self, const CatalogEntry* _Nonnull ce, did_t* _Nullable pOutId);

#endif /* IOCatalog_h */
