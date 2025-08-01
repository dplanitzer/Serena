//
//  VirtualDiskManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef VirtualDiskManager_h
#define VirtualDiskManager_h

#include <driver/Driver.h>


final_class(VirtualDiskManager, Driver);

extern errno_t VirtualDiskManager_Create(DriverRef _Nullable * _Nonnull pOutSelf);

extern errno_t VirtualDiskManager_CreateRamDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount);
extern errno_t VirtualDiskManager_CreateRomDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image);

#endif /* VirtualDiskManager_h */
