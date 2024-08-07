//
//  delay.c
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


static const char* ms_str;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("delay <ms>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&ms_str, "expected a ms duration value")
);


static errno_t do_delay(const char* _Nonnull spec)
{
    decl_try_err();
    const char* ep = NULL;
    const long ms = strtol(ms_str, &ep, 10);
    
    if (*ep != '\0') {
        return EINVAL;
    }

    return Delay(TimeInterval_MakeMilliseconds(ms));
}

int cmd_delay(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    ms_str = "";
    
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_delay(ms_str));
    }
    else {
        exitCode = clap_exit_code(status);
    }    
    
    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
