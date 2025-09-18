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

_Noreturn _fatalException(const excpt_frame_t* _Nonnull efp)
{
    vcpu_t vp = vcpu_current();

    fatal("Exception: %hhx, Format %hhx, PC %p, SR %hx\n Exception Frame: %p\n Stack Base: %p\n VCPU: %p (%d)\n, Process: %p (%d)", 
        excpt_frame_getvecnum(efp),
        excpt_frame_getformat(efp),
        excpt_frame_getpc(efp),
        excpt_frame_getsr(efp),
        efp,
        stk_getinitialsp(&vp->kernel_stack),
        vp,
        vp->id,
        vp->proc,
        Process_GetId(vp->proc));
}
