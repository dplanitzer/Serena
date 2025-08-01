//
//  DriverManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverManager_h
#define DriverManager_h

#include <filemanager/ResolvedPath.h>
#include <kern/errno.h>
#include <kern/types.h>
#include <kobj/AnyRefs.h>

typedef struct DriverManager {
    DriverRef   platformController;
    DriverRef   hidDriver;
    DriverRef   logDriver;
    DriverRef   nullDriver;
    DriverRef   virtualDiskDriver;
} DriverManager;


extern DriverManagerRef gDriverManager;


extern errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf);

extern errno_t DriverManager_Start(DriverManagerRef _Nonnull self);


// Returns the filesystem that represents the /dev catalog.
extern FilesystemRef _Nonnull DriverManager_GetCatalog(DriverManagerRef _Nonnull self);


extern errno_t DriverManager_Open(DriverManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t DriverManager_HasDriver(DriverManagerRef _Nonnull self, const char* _Nonnull path);

extern errno_t DriverManager_AcquireNodeForPath(DriverManagerRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp);

#endif /* DriverManager_h */
