//
//  kern/assert.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KERN_ASSERT_H
#define _KERN_ASSERT_H 1

#include <assert.h>
#include <System/_cmndef.h>

// defined in diskimage.c
extern void fatal(const char* _Nonnull format, ...);

#endif /* _KERN_ASSERT_H */
