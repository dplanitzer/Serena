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

extern int excpt_sethandler(int scope, int flags, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);

#endif /* _SYS_EXCEPTION_H */
