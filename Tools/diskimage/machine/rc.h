//
//  rc.h
//  diskimage
//
//  Created by Dietmar Planitzer on 7/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _DI_RC_H
#define _DI_RC_H

#include <kern/types.h>
#include <stdbool.h>
#include <Windows.h>

// The rc_xxx functions below implement a preemption and interrupt safe retain
// count mechanism.
typedef LONG ref_count_t;


#define RC_INIT 1

// Atomically increments the retain count 'rc'.
extern void rc_retain(volatile ref_count_t* _Nonnull rc);

// Atomically releases a single strong reference and adjusts the retain count
// accordingly. Returns true if the retain count has reached zero and the caller
// should destroy the associated resources.
extern bool rc_release(volatile ref_count_t* _Nonnull rc);

// Returns a copy of the current retain count. This should be used for debugging
// purposes only.
extern int rc_getcount(volatile ref_count_t* _Nonnull rc);

#endif /* _DI_RC_H */
