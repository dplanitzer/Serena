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

extern DevFSRef _Nonnull DriverCatalog_GetDevicesFilesystem(DriverCatalogRef _Nonnull self);

// Publish the driver instance 'driver' with the name 'name' in the driver
// catalog. Returns a suitable error if another entry with the same name already
// exists. 'arg' is an optional argument that will be passed to the Driver_Open()
// method when the driver needs to be opened.
extern errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Nonnull driver, intptr_t arg, DriverCatalogId* _Nonnull pOutDriverCatalogId);

// Removes a published driver entry from the driver catalog.
extern errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverCatalogId driverCatalogId);

// Returns EOK if a driver is published at the in-kernel path 'path'. Otherwise
// ENOENT is returned.
extern errno_t DriverCatalog_IsDriverPublished(DriverCatalogRef _Nonnull self, const char* _Nonnull path);

// Opens the driver at the in-kernel path 'path' with mode 'mode' and returns the
// resulting channel in 'pOutChannel'. The in-kernel path of a driver is of the
// form '/driver-name'.
extern errno_t DriverCatalog_OpenDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* DriverCatalog_h */
