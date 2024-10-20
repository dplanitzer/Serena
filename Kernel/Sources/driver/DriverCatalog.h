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

extern errno_t DriverCatalog_Publish(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverId driverId, DriverRef _Consuming _Nonnull driver);
extern void DriverCatalog_Unpublish(DriverCatalogRef _Nonnull self, DriverId driverId);

extern DriverRef DriverCatalog_CopyDriverForName(DriverCatalogRef _Nonnull self, const char* pName);
extern DriverRef DriverCatalog_CopyDriverForDriverId(DriverCatalogRef _Nonnull self, DriverId driverId);


// Generates a new and unique driver ID that should be used to publish a driver.
extern DriverId GetNewDriverId(void);

#endif /* DriverCatalog_h */
