//
//  Assert.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Assert.h"
#include <log/Log.h>


static void stop_machine()
{
    chipset_reset();
}

_Noreturn fatal(const char* _Nonnull format, ...)
{
    va_list ap;
    va_start(ap, format);

    stop_machine();

    if (log_switch_to_console()) {
        vprintf(format, ap);
    }
    else {
        cpu_non_recoverable_error();
    }
    va_end(ap);

    while (true);
    /* NOT REACHED */
}

_Noreturn fatalError(const char* _Nonnull filename, int line, int err)
{
    stop_machine();
    fatal("Fatal Error: %d at %s:%d", err, filename, line);
}

_Noreturn fatalAbort(const char* _Nonnull filename, int line)
{
    stop_machine();
    fatal("Abort: %s:%d", filename, line);
}

_Noreturn fatalAssert(const char* _Nonnull filename, int line)
{
    stop_machine();
    fatal("Assert: %s:%d", filename, line);
}

_Noreturn _fatalException(const ExceptionStackFrame* _Nonnull pFrame)
{
    stop_machine();
    fatal("Exception: %hhx, Format %hhx, PC %p, SR %hx", pFrame->fv.vector >> 2, pFrame->fv.format, pFrame->pc, pFrame->sr);
}
