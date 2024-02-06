//
//  malloc.h
//  Apollo
//
//  Created by Dietmar Planitzer on 1/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _MALLOC_H
#define _MALLOC_H 1

#include <abi/_cmndef.h>
#include <abi/_nulldef.h>
#include <apollo/_sizedef.h>

__CPP_BEGIN

extern void *malloc(size_t size);
extern void *calloc(size_t num, size_t size);
extern void *realloc(void *ptr, size_t new_size);
extern void free(void *ptr);


//
// Debugging tools.
//

// Prints a description of the heap to the console.
extern void malloc_dump(void);

__CPP_END

#endif /* _MALLOC_H */
