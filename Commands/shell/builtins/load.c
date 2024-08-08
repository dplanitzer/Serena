//
//  load.c
//  sh
//
//  Created by Dietmar Planitzer on 8/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <clap.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("load <path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a file path")
);


static errno_t do_load(InterpreterRef _Nonnull ip, const char* _Nonnull path, const char* _Nonnull proc_name)
{
    decl_try_err();
    const char* text = NULL;
    size_t len = 0;

    err = read_contents_of_file(path, &text, &len);
    if (err == EOK) {
        err = OpStack_PushString(ip->opStack, text, len);
        free(text);
    }

    if (err != EOK) {
        print_error(proc_name, path, err);
    }

    return err;
}

int cmd_load(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_load(ip, path, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }

    return exitCode;
}
