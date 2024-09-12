//
//  DriverCatalog.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverCatalog_h
#define DriverCatalog_h

#include <klib/klib.h>
#include "Driver.h"

struct DriverCatalog;
typedef struct DriverCatalog* DriverCatalogRef;


extern errno_t DriverCatalog_Create(DriverCatalogRef _Nullable * _Nonnull pOutSelf);
extern void DriverCatalog_Destroy(DriverCatalogRef _Nullable self);

extern errno_t DriverCatalog_RegisterDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull name, DriverRef _Consuming _Nonnull driver);
extern void DriverCatalog_UnregisterDriver(DriverCatalogRef _Nonnull self, const char* _Nonnull name);

extern DriverRef DriverCatalog_GetDriverForName(DriverCatalogRef _Nonnull self, const char* pName);

#endif /* DriverCatalog_h */
