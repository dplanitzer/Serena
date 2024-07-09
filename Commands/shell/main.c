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

static clap_string_array_t scriptFiles = {NULL, 0};

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("shell [path ...]"),

    CLAP_VARARG(&scriptFiles)
);


void main_closure(int argc, char *argv[])
{
    decl_try_err();
    ShellRef pShell = NULL;

    clap_parse(0, params, argc, argv);
    const bool isInteractive = (scriptFiles.count == 0) ? true : false;

    try(Shell_Create(isInteractive, &pShell));

    if (isInteractive) {
        // XXX disabled insertion mode for now because the line reader doesn't support
        // XXX it properly yet
        //printf("\033[4h");  // Switch the console to insert mode
        fputs("\n\033[36mSerena Shell v0.1.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n", stdout);

        try(Shell_Run(pShell));
    }
    else {
        for (size_t i = 0; i < scriptFiles.count; i++) {
            err = Shell_RunContentsOfFile(pShell, scriptFiles.strings[i]);
            if (err != EOK) {
                print_error(argv[0], scriptFiles.strings[i], err);
                throw(err);
            }
        }
    }

catch:
    Shell_Destroy(pShell);
    exit((err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE);
}
