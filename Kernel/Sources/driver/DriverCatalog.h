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

extern errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Nonnull driver, DriverId* _Nonnull pOutDriverId);
extern errno_t DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverId driverId);

extern DriverRef _Nullable DriverCatalog_CopyDriverForDriverId(DriverCatalogRef _Nonnull self, DriverId driverId);

// Returns EOK if a driver is published at the in-kernel path 'path'. Otherwise
// ENOENT is returned.
extern errno_t DriverCatalog_IsDriverPublished(DriverCatalogRef _Nonnull self, const char* _Nonnull path);

// Opens the driver at the in-kernel path 'path' with mode 'mode' and returns the
// resulting channel in 'pOutChannel'. The in-kernel path of a driver is of the
// form '/driver-name'.
extern errno_t DriverCatalog_OpenDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* DriverCatalog_h */
