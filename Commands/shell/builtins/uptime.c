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


static errno_t do_uptime(InterpreterRef _Nonnull ip)
{
    return OpStack_PushInteger(ip->opStack, TimeInterval_GetMillis(MonotonicClock_GetTime()));
}

int cmd_uptime(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_uptime(ip));
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return exitCode;
}
