//
//  __synch.h
//  libc
//
//  Created by Dietmar Planitzer on 1/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYNCH_H
#define __SYNCH_H 1

#include <errno.h>
#include <ext/timespec.h>
#include <serena/cnd.h>
#include <serena/mtx.h>
#include <serena/waitqueue.h>

__CPP_BEGIN

#define CND_SIGNATURE 0x53454d41
#define MTX_SIGNATURE 0x4c4f434b

extern int __mtx_unlock(mtx_t* _Nonnull self);

__CPP_END

#endif /* __SYNCH_H */
