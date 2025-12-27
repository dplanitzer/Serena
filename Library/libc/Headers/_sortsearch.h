//
//  _sortsearch.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __SORTSEARCH_H
#define __SORTSEARCH_H 1

#include <_cmndef.h>
#include <arch/_size.h>

__CPP_BEGIN

extern void* bsearch(const void *key, const void *values, size_t count, size_t size,
                        int (*comp)(const void*, const void*));
extern void qsort(void* values, size_t count, size_t size,
                        int (*comp)(const void*, const void*));

__CPP_END

#endif /* __SORTSEARCH_H */
