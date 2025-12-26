//
//  dispatch_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/3/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <dispatch.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ext/timespec.h>
#include "Asserts.h"

static dispatch_t gDispatcher;
static int gCounter;

static struct timespec DELAY_500MS;
static struct timespec DELAY_1000MS;


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_async_test

static void OnAsync(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
    //struct timespec dur;
    // timespec_from_sec(&dur, 2);
    //clock_nanosleep(clock_uptime, 0, &dur, NULL);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, NULL));
}

void dq_async_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_sync_test

static int OnSync(void* _Nonnull ign)
{
    struct timespec dur;

    timespec_from_ms(&dur, 500);
    clock_nanosleep(CLOCK_MONOTONIC, 0, &dur, NULL);
    printf("%d (Dispatcher: %p)\n", gCounter++, dispatch_current_queue());
    return 1234;
}

void dq_sync_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    while (true) {
        const int r = dispatch_sync(gDispatcher, (dispatch_sync_func_t)OnSync, NULL);

        assertEquals(1234, r);
        puts("--------");
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_after_test

static void OnAfter(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
    assertOK(dispatch_after(gDispatcher, 0, &DELAY_500MS, (dispatch_async_func_t)OnAfter, NULL));
}

void dq_after_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_500MS, 500);
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAfter, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_repeating_test

static struct timespec DELAY_250MS;

static void OnRepeating(void* _Nonnull ign)
{
    printf("%d\n", gCounter++);
}

void dq_repeating_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_250MS, 250);
    assertOK(dispatch_repeating(gDispatcher, 0, &DELAY_250MS, &DELAY_250MS, (dispatch_async_func_t)OnRepeating, NULL));
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_terminate_test

static void OnAsync2(intptr_t _Nonnull value)
{
    printf("async: %d\n", (int)value);
}

static void OnRepeating2(intptr_t _Nonnull value)
{
    printf("timer: %d\n", (int)value);
}

// Should print async 1 - 3 and then terminate
void dq_terminate_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    timespec_from_ms(&DELAY_500MS, 500);
    assertOK(dispatch_repeating(gDispatcher, 0, &DELAY_500MS, &DELAY_500MS, (dispatch_async_func_t)OnRepeating2, (void*)1));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)1));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)2));
    assertOK(dispatch_async(gDispatcher, (dispatch_async_func_t)OnAsync2, (void*)3));

    printf("Terminating...\n");
    dispatch_terminate(gDispatcher, 0);
    assertOK(dispatch_await_termination(gDispatcher));
    printf("Terminated.\n");
    assertOK(dispatch_destroy(gDispatcher));
    printf("Success!\n");
}


////////////////////////////////////////////////////////////////////////////////
// MARK: dq_signal_test

struct siginfo {
    vcpuid_t    group_id;
    int         signo;
};

static void OnReceivedSignal(intptr_t _Nonnull value)
{
    printf("   Received signal\n");
}

static void OnSendSignal(struct siginfo* _Nonnull si)
{
    static int sig_send_toggle;

    if (sig_send_toggle) {
        printf("Sending signal #%d   [sigsend]\n", si->signo);
        assertOK(sigsend(SIG_SCOPE_VCPU_GROUP, si->group_id, si->signo));
    }
    else {
        printf("Sending signal #%d   [dispatch_send_signal]\n", si->signo);
        assertOK(dispatch_send_signal(gDispatcher, si->signo));
    }

    sig_send_toggle = !sig_send_toggle;
}

// Should print 'Sending signal' and 'Received signal' once every second
void dq_signal_test(int argc, char *argv[])
{
    dispatch_attr_t attr = DISPATCH_ATTR_INIT_SERIAL_INTERACTIVE;
    gDispatcher = dispatch_create(&attr);
    assertNotNULL(gDispatcher);

    struct siginfo si;
    si.group_id = dispatch_signal_target(gDispatcher);
    si.signo = dispatch_alloc_signal(gDispatcher, 0);
    assertTrue(si.signo >= SIGMIN && si.signo <= SIGMAX);

    printf("vcpu-group-id: %u, signo: %d\n\n", si.group_id, si.signo);


    dispatch_item_t item = calloc(1, sizeof(struct dispatch_item));
    assertNotNULL(item);

    item->func = (dispatch_item_func_t)OnReceivedSignal;
    item->retireFunc = (dispatch_retire_func_t)free;

    assertOK(dispatch_item_on_signal(gDispatcher, si.signo, item));


    timespec_from_ms(&DELAY_1000MS, 1000);
    assertOK(dispatch_repeating(gDispatcher, 0, &TIMESPEC_ZERO, &DELAY_1000MS, (dispatch_async_func_t)OnSendSignal, &si));
}
