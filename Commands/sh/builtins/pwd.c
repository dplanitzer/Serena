//
//  pwd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int cmd_pwd(InterpreterRef _Nonnull self, int argc, char** argv)
{
    if (argc > 1) {
        printf("%s: unexpected extra arguments\n", argv[0]);
    }

    const errno_t err = getcwd(self->pathBuffer, PATH_MAX);
    if (err == 0) {
        printf("%s\n", self->pathBuffer);
    } else {
        printf("%s: %s.\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
