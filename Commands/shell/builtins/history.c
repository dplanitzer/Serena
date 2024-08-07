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


int cmd_history(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    int count = Interpreter_GetHistoryCount(ip);

    // We don't want to print the 'history' command if it is on top of the history
    // stack because this means that the user has just entered it to get the history.
    if (count > 0 && !strncmp(Interpreter_GetHistoryAt(ip, count - 1), "history", 7)) {
        count--;
    }

    for (int i = count - 1; i >= 0; i--) {
        puts(Interpreter_GetHistoryAt(ip, i));
    }

    // XXX should push an Array<String> 
    OpStack_PushVoid(ip->opStack);
    return EXIT_SUCCESS;
}
