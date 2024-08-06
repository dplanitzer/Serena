//
//  uptime.c
//  sh
//
//  Created by Dietmar Planitzer on 8/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdlib.h>
#include <clap.h>
#include <System/Clock.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("uptime")
);


int cmd_uptime(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }    
    

    const TimeInterval ti = MonotonicClock_GetTime();
    const mseconds_t ms = TimeInterval_GetMillis(ti);
    printf("%ld\n", ms);

    return EXIT_SUCCESS;
}
