//
//  excpt_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/03/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <serena/exception.h>
#include <serena/vcpu.h>
#include "Asserts.h"


////////////////////////////////////////////////////////////////////////////////
// excpt_crash_test

int movesr(void) = "\tmove.w\tsr, d0\n";

void excpt_crash_test(int argc, char *argv[])
{
    const int r = movesr();
    // -> process should have exited with an exception status
    // -> should not print
    printf("sr: %d\n", r);
}


////////////////////////////////////////////////////////////////////////////////
// excpt_handler_test

static int ex_handler(void* arg, const excpt_info_t* _Nonnull ei)
{
    if (ei->code == EXCPT_PRIV_INSTRUCTION) {
        printf("arg: %s\n", arg);
        printf("code: %d\n", ei->code);
        printf("cpu_code: %d\n", ei->cpu_code);
        printf("addr: %p\n", ei->fault_addr);
        printf("PC: %p, SP: %p\n", ei->pc, ei->sp);

        _Exit(EXIT_SUCCESS);
        /* NOT REACHED */
    }

    return EXCPT_ABORT_EXECUTION;
}

void excpt_handler_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler;
    h.arg = "exiting from handler";
    excpt_sethandler(EXCPT_FLAG_PROC, &h, NULL);
    
    const int r = movesr();
    // -> process should have exited with (regular) status 0
    // -> should not print
    printf("sr: %d\n", r);
}


////////////////////////////////////////////////////////////////////////////////
// excpt_return_test

static int ex_handler2(void* arg, const excpt_info_t* _Nonnull ei)
{
    if (ei->code == EXCPT_PRIV_INSTRUCTION) {
        vcpu_t vp_me = vcpu_self();
        vcpu_state_m68k_t   regs;

        printf("arg: %s\n", arg);
        printf("code: %d\n", ei->code);
        printf("cpu_code: %d\n", ei->cpu_code);
        printf("addr: %p\n", ei->fault_addr);
        printf("PC: %p, SP: %p\n", ei->pc, ei->sp);

        assert_ok(vcpu_state(vp_me, VCPU_STATE_EXCPT_M68K, &regs));

        regs.pc += 2;        // Skip the 'move sr, d0'
        regs.d[0] = 1234;    // Return a faked result

        assert_ok(vcpu_setstate(vp_me, VCPU_STATE_EXCPT_M68K, &regs));

        return EXCPT_CONTINUE_EXECUTION;
    }

    return EXCPT_ABORT_EXECUTION;
}

void excpt_return_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler2;
    h.arg = "returning from handler";
    excpt_sethandler(0, &h, NULL);
    
    const int r = movesr();
    // -> process should have returned from ex_handler2
    // -> should skip the move SR, execute the next line and print '1234'.
    printf("\nSR: %d\nExiting.\n", r);
}


////////////////////////////////////////////////////////////////////////////////
// excpt_raise_test

static int ex_handler_raised(void* arg, const excpt_info_t* _Nonnull ei)
{
    if (ei->code == EXCPT_PAGE_ERROR) {
        printf("arg: %s\n", arg);
        printf("code: %d\n", ei->code);
        printf("cpu_code: %d\n", ei->cpu_code);
        printf("addr: %p\n", ei->fault_addr);
        printf("PC: %p, SP: %p\n", ei->pc, ei->sp);

        _Exit(EXIT_SUCCESS);
        /* NOT REACHED */
    }

    return EXCPT_ABORT_EXECUTION;
}

void excpt_raise_test(int argc, char *argv[])
{
    excpt_handler_t h;

    h.func = ex_handler_raised;
    h.arg = "raised PAGE_ERROR";
    excpt_sethandler(0, &h, NULL);
    
    excpt_raise(EXCPT_PAGE_ERROR, (void*)0x1234);
    
    // -> process should have exited with (regular) status 0
    // -> should not print
    printf("bad\n");
}
