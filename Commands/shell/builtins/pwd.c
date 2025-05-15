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
#include <clap.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("pwd")
);


static int do_pwd(InterpreterRef _Nonnull ip, const char* _Nonnull proc_name)
{
    decl_try_err();
    char* buf = malloc(PATH_MAX);

    if (buf) {
        if (getcwd(buf, PATH_MAX) == 0) {
            err = OpStack_PushCString(ip->opStack, buf);
        }
        else {
            err = errno;
            print_error(proc_name, NULL, err);
        }

        free(buf);
    }

    return err;
}

int cmd_pwd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = do_pwd(ip, argv[0]);
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }

    return exitCode;
}
