//
//  ext/atomic.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 6/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_ATOMIC_H
#define _EXT_ATOMIC_H 1

#include <_cmndef.h>
#include <arch/_null.h>
#include <stdbool.h>

__CPP_BEGIN

typedef unsigned char atomic_flag;

#define ATOMIC_FLAG_INIT    0


extern bool atomic_flag_test_and_set(volatile atomic_flag* _Nonnull flag);
extern void atomic_flag_clear(volatile atomic_flag* _Nonnull flag);

__CPP_END

#endif /* _EXT_ATOMIC_H */
