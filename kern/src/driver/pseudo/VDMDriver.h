//
//  VDMDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef VDMDriver_h
#define VDMDriver_h

#include <driver/Driver.h>

#define VDM_TYPE_RAM        0   /* R/W RAM disk */
#define VDM_TYPE_REF_ROM    1   /* ROM disk which references static & constant data */


final_class(VDMDriver, Driver);
class_ref(VDMDriver);


extern errno_t VDMDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

extern errno_t VDMDriver_CreateDisk(VDMDriverRef _Nonnull self, int type, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nullable image);

#endif /* VDMDriver_h */
