//
//  uptime.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <serena/clock.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("uptime")
);


int cmd_uptime(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    nanotime_t ti;
    int r;

    clap_parse(0, params, argc, argv);

    if (clock_time(CLOCK_MONOTONIC, &ti) == 0) {
        const time_t h = ti.tv_sec / 3600;
        const time_t minsAsSecs = ti.tv_sec - h * 3600;
        const time_t m = minsAsSecs / 60;
        const time_t s = minsAsSecs - m * 60;

        printf("Uptime: %.2ld:%.2ld:%.2ld\n", h, m, s);
        r = EXIT_SUCCESS;
    }
    else {
        r = EXIT_FAILURE;
    }

    OpStack_PushVoid(ip->opStack);
    return r;
}
