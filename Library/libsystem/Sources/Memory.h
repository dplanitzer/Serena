//
//  Memory.h
//  libsystem
//
//  Created by Dietmar Planitzer on 4/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_MEMORY_H
#define __SYS_MEMORY_H 1

#include <System/_cmndef.h>

__CPP_BEGIN

extern void *__Memset(void *dst, int c, size_t count);
extern void *__Memcpy(void * _Restrict dst, const void * _Restrict src, size_t count);
extern void *__Memmove(void * _Restrict dst, const void * _Restrict src, size_t count);

__CPP_END

#endif /* __SYS_MEMORY_H */
