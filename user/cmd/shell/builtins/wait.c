//
//  wait.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <clap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ext/nanotime.h>
#include <serena/clock.h>


typedef struct tim_conv {
    const char*     unit;
    time_t          unitToMillis;
} tim_conv_t;

static const tim_conv_t gConvTable[] = {
    {"ms", 1l},
    {"s", 1000l},
    {"m", 60l * 1000l},
    {"h", 60l * 60l * 1000l},
    {"", -1l}
};


static const char* ms_str = "";

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("wait <duration>[h,m,s,ms]"),

    CLAP_REQUIRED_POSITIONAL_STRING(&ms_str, "expected a time duration value")
);


static time_t parse_time(const char* str, char** str_end)
{
    const time_t t = strtol(str, str_end, 10);

    if (**str_end == '\0') {
        return t * 1000l;
    }
    else {
        const tim_conv_t* cp = gConvTable;

        while (cp->unitToMillis > 0) {
            if (!strcmp(*str_end, cp->unit)) {
                return t * cp->unitToMillis;
            }

            cp++;
        }
    }

    return -1l;
}

int cmd_wait(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    ms_str = "";

    clap_parse(0, params, argc, argv);

    
    char* ep = "";
    const time_t ms = parse_time(ms_str, &ep);
    int r = EXIT_SUCCESS;

    if (ms > 0) {
        nanotime_t dur;
        
        nanotime_from_ms(&dur, ms);
        clock_wait(CLOCK_MONOTONIC, 0, &dur, NULL);
    }
    else if (ms < 0) {
        printf("%s: unknown time unit '%s'\n", argv[0], ep);
        r = EXIT_FAILURE;
    }

    OpStack_PushVoid(ip->opStack);
    return r;
}
