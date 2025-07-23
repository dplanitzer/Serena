//
//  main.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <clap.h>
#include "Utilities.h"
#include "Shell.h"

static clap_string_array_t arg_strings = {NULL, 0};
static bool arg_isCommand = false;
static bool arg_isLogin = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shell [path ...]"),

    CLAP_BOOL('c', "command", &arg_isCommand, "tells the shell to interpret the provided string as a command"),
    CLAP_BOOL('l', "login", &arg_isLogin, "tells the shell that it is the login shell"),
    CLAP_VARARG(&arg_strings)
);


int main(int argc, char *argv[])
{
    _abort_on_nomem();

    clap_parse(0, params, argc, argv);
    const bool isInteractive = (arg_strings.count == 0) ? true : false;

    
    // Enable SIGCHILD reception
    sigroute(SIG_SCOPE_VCPU, 0, SIG_ROUTE_ENABLE);


    ShellRef sh = Shell_Create(isInteractive);
    if (isInteractive) {
        // XXX disabled insertion mode for now because the line reader doesn't support
        // XXX it properly yet
        //printf("\033[4h");  // Switch the console to insert mode
        if (!arg_isLogin) {
            fputs("\n\033[36mSerena Shell v0.5.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n", stdout);
        }

        Shell_Run(sh);
    }
    else {
        // XXX arg_strings[1]... should be passed as arguments to the shell script/command
        int hasError;

        if (arg_isCommand) {
            hasError = Shell_RunContentsOfString(sh, arg_strings.strings[0]);
        }
        else {
            hasError = Shell_RunContentsOfFile(sh, arg_strings.strings[0]);
        }

        if (hasError) {
            print_error(argv[0], arg_strings.strings[0], errno);
        }
    }
    Shell_Destroy(sh);

    exit((errno == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
}
