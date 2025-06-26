//
//  sys/atomic.h
//  libc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ATOMIC_H
#define _SYS_ATOMIC_H 1

#include <_cmndef.h>
#include <_null.h>
#include <stdbool.h>

__CPP_BEGIN

typedef unsigned char atomic_flag;

#define ATOMIC_FLAG_INIT    0


extern bool atomic_flag_test_and_set(volatile atomic_flag* _Nonnull flag);
extern void atomic_flag_clear(volatile atomic_flag* _Nonnull flag);

__CPP_END

#endif /* _SYS_ATOMIC_H */
