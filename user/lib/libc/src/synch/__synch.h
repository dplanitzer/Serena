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
#include <ext/nanotime.h>
#include <serena/cnd.h>
#include <serena/mtx.h>
#include <serena/sem.h>
#include <serena/waitqueue.h>
#include <serena/synch.h>
#include <stdbool.h>

__CPP_BEGIN

//
// The sem implementation is inspired by these articles:
// https://www.remlab.net/op/futex-misc.shtml
// https://developers.redhat.com/articles/2023/04/18/implementing-c20-semaphores#ok__so_how_to_implement_it_
//
// Known issues and limitations:
// - sem: doesn't handle value overflow correctly. Very unlikely that anyone
//        will ever run into this. Should eventually be fixed though by returning
//        a suitable error instead of trying to do a sem_post()

#define CND_SIGNATURE 0x53454d41
#define MTX_SIGNATURE 0x4c4f434b
#define SEM_SIGNATURE 0x53454d41

extern int __mtx_unlock(mtx_t* _Nonnull self);
extern bool __sem_trywait(sem_t* _Nonnull self);

__CPP_END

#endif /* __SYNCH_H */
