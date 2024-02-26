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


int cmd_history(InterpreterRef _Nonnull self, int argc, char** argv)
{
    int count = ShellContext_GetHistoryCount(self->context);

    // We don't want to print the 'history' command if it is on top of the history
    // stack because this means that the user has just entered it to get the history.
    if (count > 0 && !strncmp(ShellContext_GetHistoryAt(self->context, count - 1), "history", 7)) {
        count--;
    }

    for (int i = count - 1; i >= 0; i--) {
        puts(ShellContext_GetHistoryAt(self->context, i));
    }

    return EXIT_SUCCESS;
}
