//
//  serena/exception.h
//  libc
//
//  Created by Dietmar Planitzer on 7/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SERENA_EXCEPTION_H
#define _SERENA_EXCEPTION_H 1

#include <_cmndef.h>
#include <kpi/exception.h>

// Installs 'handler' as the new exception handler and optionally returns the
// current handler in 'old_handler'. The handler is by default installed for
// just the calling vcpu. Pass EXCPT_FLAG_PROC as 'flags' to install the handler
// on the process level which means that the handler will be invoked from any
// vcpu that encounters a CPU exception.
extern int excpt_sethandler(int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);

// Synchronously raise the exception 'code' in the context of the calling vcpu.
// The exception info will list 'fault_addr' as the fault address.
extern int excpt_raise(int code, void* _Nullable fault_addr);

#endif /* _SERENA_EXCEPTION_H */
