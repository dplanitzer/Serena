//
//  DriverCatalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverCatalog_h
#define DriverCatalog_h

#include <kobj/Object.h>

extern DriverCatalogRef _Nonnull  gDriverCatalog;

extern errno_t DriverCatalog_Create(DriverCatalogRef _Nullable * _Nonnull pOutSelf);
extern void DriverCatalog_Destroy(DriverCatalogRef _Nullable self);

extern FilesystemRef _Nonnull DriverCatalog_CopyFilesystem(DriverCatalogRef _Nonnull self);

// Publish the driver instance 'driver' with the name 'name' as a child of the
// bus directory 'busCatalogId', in the driver catalog. The device is published
// as a child of the root directory if 'busCatalogId' is kDriverCatalogId_None.
// Returns a suitable error if another entry with the same name already exists.
// 'arg' is an optional argument that will be passed to the Driver_Open() method
// when the driver needs to be opened.
extern errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, DriverCatalogId busCatalogId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverRef _Nonnull driver, intptr_t arg, DriverCatalogId* _Nonnull pOutDriverCatalogId);

// Publishes a bus directory with the name 'name' to the driver catalog.
extern errno_t DriverCatalog_PublishBus(DriverCatalogRef _Nonnull self, DriverCatalogId parentBusId, const char* _Nonnull name, uid_t uid, gid_t gid, FilePermissions perms, DriverCatalogId* _Nonnull pOutBusCatalogId);

// Either removes a published driver entry from the driver catalog or a published
// bus entry. Pass the bus and the driver catalog ID if you want to remove a
// driver entry. Note that this removes just the driver entry and not the bus
// directory. Pass a bus catalog ID and kDriverCatalog_None as the driver catalog
// ID to remove a bus directory. Note that the bus directory has to be empty in
// order to remove it.
extern errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverCatalogId busCatalogId, DriverCatalogId driverCatalogId);

// Returns EOK if a driver is published at the in-kernel path 'path'. Otherwise
// ENOENT is returned.
extern errno_t DriverCatalog_IsDriverPublished(DriverCatalogRef _Nonnull self, const char* _Nonnull path);

// Opens the driver at the in-kernel path 'path' with mode 'mode' and returns the
// resulting channel in 'pOutChannel'. The in-kernel path of a driver is of the
// form '/driver-name'.
extern errno_t DriverCatalog_OpenDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* DriverCatalog_h */
