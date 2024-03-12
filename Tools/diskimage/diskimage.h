//
//  diskimage.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef diskimage_h
#define diskimage_h

#include "diskimage_types.h"
#include "FauxDisk.h"


extern TimeInterval MonotonicClock_GetCurrentTime(void);

extern errno_t kalloc(size_t nbytes, void** pOutPtr);
extern void kfree(void* ptr);


#define Bytes_ClearRange(__p, __nbytes) \
    memset(__p, 0, __nbytes)

#endif /* diskimage_h */
