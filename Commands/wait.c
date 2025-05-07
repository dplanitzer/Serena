//
//  wait.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/Clock.h>


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

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("wait <duration>[h,m,s,ms]"),

    CLAP_REQUIRED_POSITIONAL_STRING(&ms_str, "expected a time duration value")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    const char* ep = "";
    const time_t t = strtol(ms_str, &ep, 10);
    time_t ms;

    if (*ep == '\0') {
        ms = t * 1000l;
    }
    else {
        const tim_conv_t* cp = gConvTable;
        bool ok = false;

        while (cp->unitToMillis > 0) {
            if (!strcmp(ep, cp->unit)) {
                ms = t * cp->unitToMillis;
                ok = true;
                break;
            }

            cp++;
        }

        if (!ok) {
            printf("%s: unknown time unit '%s'\n", argv[0], ep);
            return EXIT_FAILURE;    
        }
    }

    TimeInterval dur = TimeInterval_MakeMilliseconds(ms);
    clock_wait(CLOCK_UPTIME, &dur);

    return EXIT_SUCCESS;
}
