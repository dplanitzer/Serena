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


static void xxx_tmp_open_console_if_needed()
{
    int fd;

    if (IOChannel_GetMode(kIOChannel_Stdin) != 0) {
        return;
    }

    File_Open("/dev/console", kOpen_Read, &fd);
    File_Open("/dev/console", kOpen_Write, &fd);
    File_Open("/dev/console", kOpen_Write, &fd);

    fdreopen(kIOChannel_Stdin, "r", stdin);
    fdreopen(kIOChannel_Stdout, "w", stdout);
    fdreopen(kIOChannel_Stderr, "w", stderr);

    Process_SetWorkingDirectory("/Users/Administrator");
}

void main_closure(int argc, char *argv[])
{
    decl_try_err();
    ShellRef pShell = NULL;

    xxx_tmp_open_console_if_needed();

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
