//
//  _syscalldef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/2/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYSCALLS_H
#define __SYSCALLS_H 1

#define SC_write                0   // errno_t write(const char *buffer, int count)
#define SC_sleep                1   // errno_t sleep(struct {int secs, int nanosecs})
#define SC_dispatch_async       2   // errno_t dispatch_async(void *pUserClosure)
#define SC_alloc_address_space  3   // errno_t alloc_address_space(int nbytes, void **pOutMem)
#define SC_exit                 4   // _Noreturn exit(int status)
#define SC_spawn_process        5   // errno_t spawn_process(void *pUserEntryPoint)

#endif /* __SYSCALLS_H */
