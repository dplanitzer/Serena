//
//  pwd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <clap.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("pwd")
);


static void do_pwd(InterpreterRef _Nonnull ip, const char* _Nonnull proc_name)
{
    char* buf = malloc(PATH_MAX);

    getcwd(buf, PATH_MAX);
    OpStack_PushCString(ip->opStack, buf);
    free(buf);
}

int cmd_pwd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        do_pwd(ip, argv[0]);
        exitCode = EXIT_SUCCESS;
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }

    return exitCode;
}
