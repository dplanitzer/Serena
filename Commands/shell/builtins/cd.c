//
//  cd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <clap.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("cd <directory>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a path to a directory")
);


static int do_cd(const char* path, const char* proc_name)
{
    if (chdir(path) == 0) {
        return EXIT_SUCCESS;
    }
    else {
        print_error(proc_name, path, errno);
        return EXIT_FAILURE;
    }
}

int cmd_cd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = do_cd(path, argv[0]);
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
