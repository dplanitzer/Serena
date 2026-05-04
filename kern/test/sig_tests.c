//
//  sig_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 5/03/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <serena/process.h>
#include <serena/signal.h>
#include "Asserts.h"


////////////////////////////////////////////////////////////////////////////////
// sig_send_no_perm_test

void sig_send_no_perm_test(int argc, char *argv[])
{
    proc_basic_info_t info;

    proc_info(PROC_SELF, PROC_INFO_BASIC, &info);

    puts("Sending SIG_TERMINATE to kerneld pid");
    assert_nok(EPERM, sig_send(SIG_TARGET_PROC, PID_KERNELD, SIG_TERMINATE));

    puts("Sending SIG_TERMINATE to kerneld process group");
    assert_nok(EPERM, sig_send(SIG_TARGET_PROC_GROUP, PID_KERNELD, SIG_TERMINATE));

    puts("Sending SIG_TERMINATE to kerneld session");
    assert_nok(EPERM, sig_send(SIG_TARGET_SESSION, PID_KERNELD, SIG_TERMINATE));

    printf("Sending SIG_VCPU_RELINQUISH using SIG_TARGET_PROC to pid: %d (parent)\n", info.ppid);
    assert_nok(EPERM, sig_send(SIG_TARGET_PROC, info.ppid, SIG_VCPU_RELINQUISH));

    printf("Sending SIG_VCPU_RELINQUISH using SIG_TARGET_VCPU to pid: %d (parent)\n", info.ppid);
    assert_nok(EPERM, sig_send(SIG_TARGET_VCPU, info.ppid, SIG_VCPU_RELINQUISH));

    puts("Exiting");
    exit(0);
}
