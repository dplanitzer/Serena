//
//  history.c
//  sh
//
//  Created by Dietmar Planitzer on 2/25/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int cmd_history(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    int count = ShellContext_GetHistoryCount(pContext);

    // We don't want to print the 'history' command if it is on top of the history
    // stack because this means that the user has just entered it to get the history.
    if (count > 0 && !strncmp(ShellContext_GetHistoryAt(pContext, count - 1), "history", 7)) {
        count--;
    }

    for (int i = count - 1; i >= 0; i--) {
        puts(ShellContext_GetHistoryAt(pContext, i));
    }

    return EXIT_SUCCESS;
}
