//
//  VDMDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef VDMDriver_h
#define VDMDriver_h

#include <driver/pseudo/PseudoDriver.h>


final_class(VDMDriver, PseudoDriver);
class_ref(VDMDriver);


extern errno_t VDMDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

extern errno_t VDMDriver_CreateRamDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount);
extern errno_t VDMDriver_CreateRomDisk(VDMDriverRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image);

#endif /* VDMDriver_h */
