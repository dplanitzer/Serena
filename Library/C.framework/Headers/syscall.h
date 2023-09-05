//
//  _syscall.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYSCALL_H
#define _SYSCALL_H 1

#include <_cmndef.h>
#include <errno.h>

__CPP_BEGIN

#define SC_write                0   // write(const char *str)
#define SC_sleep                1   // sleep(struct {int secs, int nanosecs})
#define SC_dispatch_async       2   // dispatch_async(void *pUserClosure)
#define SC_alloc_address_space  3   // alloc_address_space(int nbytes, void **pOutMem)
#define SC_exit                 4   // exit(int status) [noreturn]
#define SC_spawn_process        5   // spawn_process(void *pUserEntryPoint)


extern int __syscall(int scno, ...);

__CPP_END

#endif /* _SYSCALL_H */
