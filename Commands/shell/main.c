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
#include "Shell.h"


void main_closure(int argc, char *argv[])
{
    decl_try_err();
    ShellRef pShell = NULL;

    // XXX disabled insertion mode for now because the line reader doesn't support
    // XXX it properly yet
    //printf("\033[4h");  // Switch the console to insert mode
    printf("\033[36mSerena OS v0.1.0-alpha\033[0m\nCopyright 2023, Dietmar Planitzer.\n\n");

    try(Shell_CreateInteractive(&pShell));
    try(Shell_Run(pShell));

catch:
    Shell_Destroy(pShell);
    exit((err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE);
}
