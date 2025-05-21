//
//  exists.c
//  sh
//
//  Created by Dietmar Planitzer on 8/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdlib.h>
#include <unistd.h>
#include <clap.h>
#include <sys/stat.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("exists <path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a path")
);


int cmd_exists(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    path = "";
    
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode = EXIT_SUCCESS;

    if (!clap_should_exit(status)) {
        OpStack_PushBool(ip->opStack, (access(path, F_OK) == 0) ? true : false);
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return exitCode;
}
