//
//  echo.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


static clap_string_array_t strs = {NULL, 0};
static bool is_noline = false;
static bool is_nospace = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("echo [-n| --noline] [-s | --nospace] <strings ...>"),

    CLAP_BOOL('n', "noline", &is_noline, "Do not output a newline"),
    CLAP_BOOL('s', "nospace", &is_nospace, "Do not output a space between arguments"),
    CLAP_VARARG(&strs)
);


static errno_t do_echo(clap_string_array_t* _Nonnull strs, bool isNoSpace, bool isNoLine)
{
    for (int i = 0; i < (int)strs->count; i++) {
        fputs(strs->strings[i], stdout);
        
        if (!isNoSpace && i < (strs->count - 2)) {
            fputc(' ', stdout);
        }
    }

    if (!isNoLine) {
        fputc('\n', stdout);
    }

    return EOK;
}

int cmd_echo(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    is_noline = false;
    is_nospace = false;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_echo(&strs, is_nospace, is_noline));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
