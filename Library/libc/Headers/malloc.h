//
//  malloc.h
//  libc
//
//  Created by Dietmar Planitzer on 1/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _MALLOC_H
#define _MALLOC_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_cmndef.h>
#include <arch/_size.h>

__CPP_BEGIN

extern void *malloc(size_t size);
extern void *calloc(size_t num, size_t size);
extern void *realloc(void *ptr, size_t new_size);
extern void free(void *ptr);

// Call this function at the beginning of your main() function to inform the
// library that all memory allocation functions (malloc, calloc, realloc) should
// terminate the process if allocating memory is unsuccessful because of a lack
// of memory.
extern void _abort_on_nomem(void);

__CPP_END

#endif /* _MALLOC_H */
