//
//  ext/atomic.h
//  diskimage
//
//  Created by Dietmar Planitzer on 12/25/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_ATOMIC_H
#define _EXT_ATOMIC_H 1

#include <_cmndef.h>
#include <stdatomic.h>

__CPP_BEGIN

#define atomic_int_store(__p, __val) \
atomic_store(__p, __val)

#define atomic_int_load(__p) \
atomic_load(__p)

#define atomic_int_exchange(__p, __val) \
atomic_exchange(__p, __val)

#define atomic_int_compare_exchange_strong(__p, __expected, __desired) \
atomic_compare_exchange_strong(__p, __expected, __desired)

#define atomic_int_fetch_add(__p, __op) \
atomic_fetch_add(__p, __op)

#define atomic_int_fetch_sub(__p, __op) \
atomic_fetch_sub(__p, __op)

#define atomic_int_fetch_or(__p, __op) \
atomic_fetch_or(__p, __op)

#define atomic_int_fetch_xor(__p, __op) \
atomic_fetch_xor(__p, __op)

#define atomic_int_fetch_and(__p, __op) \
atomic_fetch_and(__p, __op)

__CPP_END

#endif /* _EXT_ATOMIC_H */
