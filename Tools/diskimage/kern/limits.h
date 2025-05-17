//
//  kern/limits.h
//  diskimage
//
//  Created by Dietmar Planitzer on 5/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#if defined(_WIN64)
#define __OFF_WIDTH 32
#define __OFF_MIN 0x80000000ll
#define __OFF_MAX 0x7fffffffll

#define __SIZE_WIDTH 64
#define __SIZE_MAX 18446744073709551615ul

#define __SSIZE_WIDTH 64
#define __SSIZE_MIN 0x8000000000000000l
#define __SSIZE_MAX 0x7fffffffffffffffl
#else
#error "unknown platform"
#endif

#include <../../Library/libc/Headers/kern/limits.h>
