//
//  sys/exception.h
//  libc
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_EXCEPTION_H
#define _SYS_EXCEPTION_H 1

#include <_cmndef.h>
#include <kpi/exception.h>

extern int excpt_sethandler(int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);

// Synchronously raise the exception 'code' in the context of the calling vcpu.
// 'fault_addr' is currently not supported. 
extern int excpt_raise(int code, void* _Nullable fault_addr);

#endif /* _SYS_EXCEPTION_H */
