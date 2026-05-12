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
    proc_ids_info_t info;

    proc_info(PROC_SELF, PROC_INFO_IDS, &info);

    puts("Sending SIG_FORCE_QUIT to kerneld pid");
    assert_nok(EPERM, sig_send(SIG_TARGET_PROC, PID_KERNELD, SIG_FORCE_QUIT));

    puts("Sending SIG_FORCE_QUIT to kerneld process group");
    assert_nok(EPERM, sig_send(SIG_TARGET_PROC_GROUP, PID_KERNELD, SIG_FORCE_QUIT));

    puts("Sending SIG_FORCE_QUIT to kerneld session");
    assert_nok(EPERM, sig_send(SIG_TARGET_SESSION, PID_KERNELD, SIG_FORCE_QUIT));

    puts("Exiting");
    exit(0);
}
