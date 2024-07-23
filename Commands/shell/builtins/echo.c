//
//  echo.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


static void echo_string(const char* _Nonnull str)
{
    fputs(str, stdout);
}


////////////////////////////////////////////////////////////////////////////////

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


int cmd_echo(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    is_noline = false;
    is_nospace = false;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }

    for (int i = 0; i < (int)strs.count; i++) {
        echo_string(strs.strings[i]);
        
        if (!is_nospace && i < (argc - 2)) {
            fputc(' ', stdout);
        }
    }

    if (!is_noline) {
        fputc('\n', stdout);
    }

    return EXIT_SUCCESS;
}
