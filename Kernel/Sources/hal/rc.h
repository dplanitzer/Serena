//
//  rc.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _RC_H
#define _RC_H

#include <_cmndef.h>
#include <stdbool.h>


// The rc_xxx functions below implement a preemption and interrupt safe retain
// count mechanism.
typedef int ref_count_t;


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

#endif /* _RC_H */
