//
//  vdm.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef vdm_h
#define vdm_h

#include <kern/kernlib.h>
#include <kpi/types.h>

#define VDM_TYPE_RAM        0   /* R/W RAM disk */
#define VDM_TYPE_REF_ROM    1   /* ROM disk which references static & constant data */


extern errno_t vdm_create_disk(int type, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nullable image);

#endif /* vdm_h */
