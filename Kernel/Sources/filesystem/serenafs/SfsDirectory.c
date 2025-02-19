//
//  SfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsDirectory.h"
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>
#include <System/ByteOrder.h>




class_func_defs(SfsDirectory, SfsFile,
//override_func_def(onStart, ZRamDriver, Driver)
);
