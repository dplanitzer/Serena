//
//  fatal.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/16/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <hal/cpu.h>
#include <kern/kernlib.h>
#include <kern/log.h>
#include <kdispatch/kdispatch.h>
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

void _Assert_failed0(void)
{
    abort();
}

void _Assert_failed1(int lineno, const char* _Nonnull _Restrict funcname)
{
    fatal("%s:%d: assertion failed.\n", funcname, lineno);
    abort();
}

void _Assert_failed2(int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr)
{
    fatal("%s:%d: assertion '%s' failed.\n", funcname, lineno, expr);
    abort();
}

void _Assert_failed3(const char* _Nonnull _Restrict filename, int lineno, const char* _Nonnull _Restrict funcname, const char* _Nonnull _Restrict expr)
{
    fatal("%s:%s:%d: assertion '%s' failed.\n", filename, funcname, lineno, expr);
    abort();
}

_Noreturn _Try_bang_failed1(int lineno, const char* _Nonnull funcname, errno_t err)
{
    fatal("Fatal Error: %d at %s:%d", err, funcname, lineno);
}

_Noreturn _Try_bang_failed2(const char* _Nonnull filename, int lineno, const char* _Nonnull funcname, errno_t err)
{
    fatal("Fatal Error: %d at %s:%s:%d", err, filename, funcname, lineno);
}

_Noreturn _Try_bang_failed0(void)
{
    abort();
}

_Noreturn _fatalException(void* _Nonnull ksp)
{
    vcpu_t vp = vcpu_current();
    const cpu_savearea_t* sa = vp->excpt_sa;
    excpt_frame_t* efp = (excpt_frame_t*)&sa->ef;
    char* sp = (excpt_frame_isuser(efp)) ? (char*)sa->usp : ksp;
    const stk_t* stk = (excpt_frame_isuser(efp)) ? &vp->user_stack : &vp->kernel_stack;
    kdispatch_t dq = kdispatch_current_queue();

    static char dq_nam[KDISPATCH_MAX_NAME_LENGTH + 1];
    static char proc_name[16 + 1];

    if (dq) {
        kdispatch_name(dq, dq_nam, KDISPATCH_MAX_NAME_LENGTH + 1);
    }
    Process_GetArgv0(vp->proc, proc_name, sizeof(proc_name));


    fatal(
        "\033[?25l\nException %hhx Format %hhx From %s  \n"
        "\n"
        "D0 %08lx D1 %08lx D2 %08lx D3 %08lx  \n"
        "D4 %08lx D5 %08lx D6 %08lx D7 %08lx  \n"
        "A0 %08lx A1 %08lx A2 %08lx A3 %08lx  \n"
        "A4 %08lx A5 %08lx A6 %08lx A7 %08lx  \n"
        "PC %08lx SR %04hx  \n"
        "\n"
        "%s %08lx - %08lx  \n"
        "EXCP %08lx  \n"
        "\n"
        "VCPU %08lx id=%d grp=%d  \n"
        "PROC %08lx id=%d name=\"%s\"  \n"
        "DISP %08lx name=\"%s\"  ",
        excpt_frame_getvecnum(efp),
        excpt_frame_getformat(efp),
        (excpt_frame_isuser(efp)) ? "USR" : "KERN",

        sa->d[0], sa->d[1], sa->d[2], sa->d[3],
        sa->d[4], sa->d[5], sa->d[6], sa->d[7],
        sa->a[0], sa->a[1], sa->a[2], sa->a[3],
        sa->a[4], sa->a[5], sa->a[6], sp,
        excpt_frame_getpc(efp), excpt_frame_getsr(efp),
        (excpt_frame_isuser(efp)) ? "USTK" : "KSTK", stk->base, stk_getinitialsp(stk),
        efp,
        vp, vp->id, vp->groupid,
        vp->proc, Process_GetId(vp->proc), proc_name,
        dq, (dq) ? dq_nam : ""
    );
}
