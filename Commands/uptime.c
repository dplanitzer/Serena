//
//  uptime.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <System/Clock.h>


CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("uptime")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    const TimeInterval ti = clock_gettime();
    const time_t h = ti.tv_sec / 3600;
    const time_t minsAsSecs = ti.tv_sec - h * 3600;
    const time_t m = minsAsSecs / 60;
    const time_t s = minsAsSecs - m * 60;

    printf("Uptime: %.2ld:%.2ld:%.2ld\n", h, m, s);

    return EXIT_SUCCESS;
}
