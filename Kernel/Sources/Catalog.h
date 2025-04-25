//
//  Catalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Catalog_h
#define Catalog_h

#include <kobj/Object.h>
#include <filemanager/ResolvedPath.h>


// The non-persistent, globally unique ID of a published catalog entry.
// This ID does not survive a system reboot. Id 0 represents a catalog
// entry that does not exist.
typedef uint32_t    CatalogId;

// Means no catalog entry
#define kCatalogId_None   0


extern CatalogRef _Nonnull  gDriverCatalog;
extern CatalogRef _Nonnull  gFSCatalog;


extern errno_t Catalog_Create(const char* _Nonnull name, CatalogRef _Nullable * _Nonnull pOutSelf);

extern FilesystemRef _Nonnull Catalog_CopyFilesystem(CatalogRef _Nonnull self);

// Returns EOK if an entry  is published at the in-kernel path 'path'. Otherwise
// ENOENT is returned.
extern errno_t Catalog_IsPublished(CatalogRef _Nonnull self, const char* _Nonnull path);

// Looks up the inode for the given path and returns it.
extern errno_t Catalog_AcquireNodeForPath(CatalogRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp);

// Opens the catalog entry at the in-kernel path 'path' with mode 'mode' and
// returns the resulting channel in 'pOutChannel'. This call does not support
// opening a folder.
extern errno_t Catalog_Open(CatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

// Returns the path for 'cid'.
extern errno_t Catalog_GetPath(CatalogRef _Nonnull self, CatalogId cid, size_t bufSize, char* _Nonnull buf);


// Publishes a folder with the name 'name' to the catalog. Pass kCatalog_None as
// the 'parentFolderId' to create the new folder inside the root folder. 
extern errno_t Catalog_PublishFolder(CatalogRef _Nonnull self, CatalogId parentFolderId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, CatalogId* _Nonnull pOutFolderId);


// Either removes a published entry or a published folder from the catalog.
// Pass both a folder ID and the entry ID if you want to remove an entry.
// Note that this removes just the entry and not the published folder. Pass a
// folder ID and kCatalog_None as the entry ID to remove a folder. Note that the
// folder must be empty in order to remove it.
extern errno_t Catalog_Unpublish(CatalogRef _Nonnull self, CatalogId folderId, CatalogId entryId);


// Publish the driver instance 'driver' with the name 'name' as a child of the
// bus directory 'folderId', in the driver catalog. The device is published
// as a child of the root directory if 'folderId' is kCatalogId_None.
// Returns a suitable error if another entry with the same name already exists.
// 'arg' is an optional argument that will be passed to the Driver_Open() method
// when the driver needs to be opened.
extern errno_t Catalog_PublishDriver(CatalogRef _Nonnull self, CatalogId folderId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverRef _Nonnull driver, intptr_t arg, CatalogId* _Nonnull pOutCatalogId);

// Publish the filesystem instance 'fs' with the name 'name' in the root
// directory of the catalog. Returns a suitable error if another entry with the
// same name already exists.
extern errno_t Catalog_PublishFilesystem(CatalogRef _Nonnull self, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, FilesystemRef _Nonnull fs, CatalogId* _Nonnull pOutCatalogId);

#endif /* Catalog_h */
