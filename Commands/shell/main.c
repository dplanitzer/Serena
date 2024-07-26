//
//  main.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <clap.h>
#include "Utilities.h"
#include "Shell.h"

static clap_string_array_t arg_strings = {NULL, 0};
static bool arg_isCommand = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shell [path ...]"),

    CLAP_BOOL('c', "command", &arg_isCommand, "tells the shell to interpret the provided string as a command"),
    CLAP_VARARG(&arg_strings)
);


void main_closure(int argc, char *argv[])
{
    decl_try_err();
    ShellRef sh = NULL;

    clap_parse(0, params, argc, argv);
    const bool isInteractive = (arg_strings.count == 0) ? true : false;

    err = Shell_Create(isInteractive, &sh);
    if (err == EOK) {
        if (isInteractive) {
            // XXX disabled insertion mode for now because the line reader doesn't support
            // XXX it properly yet
            //printf("\033[4h");  // Switch the console to insert mode
            fputs("\n\033[36mSerena Shell v0.1.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n", stdout);

            err = Shell_Run(sh);
        }
        else {
            // XXX arg_strings[1]... should be passed as arguments to the shell script/command
            if (arg_isCommand) {
                err = Shell_RunContentsOfString(sh, arg_strings.strings[0]);
            }
            else {
                err = Shell_RunContentsOfFile(sh, arg_strings.strings[0]);
            }

            if (err != EOK) {
                print_error(argv[0], arg_strings.strings[0], err);
            }
        }
        Shell_Destroy(sh);
    }

    exit((err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE);
}
