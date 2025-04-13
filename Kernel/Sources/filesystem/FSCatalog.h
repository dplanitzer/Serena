//
//  FSCatalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef FSCatalog_h
#define FSCatalog_h

#include <kobj/Object.h>

extern FSCatalogRef _Nonnull  gFSCatalog;

extern errno_t FSCatalog_Create(FSCatalogRef _Nullable * _Nonnull pOutSelf);
extern void FSCatalog_Destroy(FSCatalogRef _Nullable self);

extern FilesystemRef _Nonnull FSCatalog_CopyFilesystem(FSCatalogRef _Nonnull self);

// Publish the driver instance 'driver' with the name 'name' as a child of the
// bus directory 'busCatalogId', in the driver catalog. The device is published
// as a child of the root directory if 'busCatalogId' is kFSCatalogId_None.
// Returns a suitable error if another entry with the same name already exists.
// 'arg' is an optional argument that will be passed to the Driver_Open() method
// when the driver needs to be opened.
extern errno_t FSCatalog_Publish(FSCatalogRef _Nonnull self, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, FilesystemRef _Nonnull fs, FSCatalogId* _Nonnull pOutFSCatalogId);

// Either removes a published driver entry from the driver catalog or a published
// bus entry. Pass the bus and the driver catalog ID if you want to remove a
// driver entry. Note that this removes just the driver entry and not the bus
// directory. Pass a bus catalog ID and kFSCatalog_None as the driver catalog
// ID to remove a bus directory. Note that the bus directory has to be empty in
// order to remove it.
extern errno_t FSCatalog_Unpublish(FSCatalogRef _Nonnull self, FSCatalogId fsCatalogId);

// Opens the driver at the in-kernel path 'path' with mode 'mode' and returns the
// resulting channel in 'pOutChannel'. The in-kernel path of a driver is of the
// form '/driver-name'.
extern errno_t FSCatalog_OpenFilesystem(FSCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* FSCatalog_h */
