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


// Publishes a folder with the name 'name' to the catalog. Pass kIOCatalog_None as
// the 'parentFolderId' to create the new folder inside the root folder. 
extern errno_t IOCatalog_PublishFolder(IOCatalogRef _Nonnull self, CatalogId parentFolderId, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutFolderId);


// Either removes a published entry or a published folder from the catalog.
// Pass both a folder ID and the entry ID if you want to remove an entry.
// Note that this removes just the entry and not the published folder. Pass a
// folder ID and kIOCatalog_None as the entry ID to remove a folder. Note that the
// folder must be empty in order to remove it.
extern errno_t IOCatalog_Unpublish(IOCatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId);


// Publish the driver instance 'drv' with the name 'name' as a child of the
// directory 'folderId', in the driver catalog. The driver is published as a
// child of the root directory if 'folderId' is kCatalogId_None.
// Returns a suitable error if another entry with the same name already exists.
// 'arg' is an optional argument that will be passed to the Driver_Open() method
// when the driver needs to be opened.
extern errno_t IOCatalog_PublishDriver(IOCatalogRef _Nonnull self, DriverRef _Nonnull drv, CatalogId folderId, const DriverEntry* _Nonnull de, did_t* _Nullable pOutId);

// Publishes a new special file entry in the catalog. The entry may be associated
// with an already existing resource 'resource'. The function 'func' is called
// with 'ctx' as teh first argument when a handler for the resource is needed.
// 'resource' may be NULL. In this case the node will return NULL as its resource
// and 'func' will still be called with 'ctx' to create a handler when needed.
extern errno_t IOCatalog_PublishEntry(IOCatalogRef _Nonnull self, CatalogId folderId, const CatalogEntry* _Nonnull ce, did_t* _Nullable pOutId);

#endif /* IOCatalog_h */
