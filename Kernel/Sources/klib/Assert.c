//
//  Assert.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <kern/assert.h>
#include <kern/kernlib.h>
#include <kdispatch/kdispatch.h>
#include <log/Log.h>
#include <machine/cpu.h>
#include <process/Process.h>
#include <sched/vcpu.h>


_Noreturn vfatal(const char* _Nonnull format, va_list ap)
{
    vprintf(format, ap);
    
    if (log_switch_to_console()) {
        cpu_halt();
    }
    else {
        const char** p = NULL;

        *p = log_buffer();
        cpu_non_recoverable_error(0x039e);
    }
    /* NOT REACHED */
}

_Noreturn fatal(const char* _Nonnull format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    vfatal(format, ap);
    va_end(ap);
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

_Noreturn _fatalException(const excpt_frame_t* _Nonnull efp, void* _Nonnull ksp)
{
    vcpu_t vp = vcpu_current();
    kdispatch_t dq = kdispatch_current_queue();
    static char dq_nam[KDISPATCH_MAX_NAME_LENGTH + 1];

    if (dq) {
        kdispatch_name(dq, dq_nam, KDISPATCH_MAX_NAME_LENGTH + 1);
    }

    fatal("\033[?25l\nException: %hhx, Format %hhx, PC %p, SR %hx\n Crash in: %s\n Exception Frame: %p\n Kernel SP: %p\n VCPU: %p (%d)\n Dispatcher: %p \"%s\"\n Process: %p (%d)", 
        excpt_frame_getvecnum(efp),
        excpt_frame_getformat(efp),
        excpt_frame_getpc(efp),
        excpt_frame_getsr(efp),
        (excpt_frame_isuser(efp) ? "USER" : "KERNEL"),
        efp,
        ksp,
        vp,
        vp->id,
        dq,
        (dq) ? dq_nam : "",
        vp->proc,
        Process_GetId(vp->proc));
}
