//
//  Assert.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/assert.h>
#include <kern/kernlib.h>
#include <log/Log.h>
#include <machine/cpu.h>


_Noreturn fatal(const char* _Nonnull format, ...)
{
    va_list ap;
    va_start(ap, format);

    if (log_switch_to_console()) {
        vprintf(format, ap);
    }
    else {
        const char** p = NULL;

        vprintf(format, ap);
        *p = log_buffer();
        cpu_non_recoverable_error();
    }
    va_end(ap);

    cpu_halt();
    /* NOT REACHED */
}

_Noreturn fatalError(const char* _Nonnull filename, int line, int err)
{
    fatal("Fatal Error: %d at %s:%d", err, filename, line);
}

_Noreturn fatalAbort(const char* _Nonnull filename, int line)
{
    fatal("Abort: %s:%d", filename, line);
}

_Noreturn fatalAssert(const char* _Nonnull filename, int line)
{
    fatal("Assert: %s:%d", filename, line);
}

_Noreturn _fatalException(const excpt_frame_t* _Nonnull efp)
{
    fatal("Exception: %hhx, Format %hhx, PC %p, SR %hx", 
        excpt_frame_getvecnum(efp),
        excpt_frame_getformat(efp),
        excpt_frame_getpc(efp),
        excpt_frame_getsr(efp));
}
