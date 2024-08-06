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


int cmd_delay(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }    
    
    const char* ep = NULL;
    const long ms = strtol(ms_str, &ep, 10);
    if (*ep != '\0') {
        return EXIT_FAILURE;
    }

    return exit_code(Delay(TimeInterval_MakeMilliseconds(ms)));
}
