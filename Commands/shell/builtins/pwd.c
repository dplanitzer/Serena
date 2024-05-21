//
//  pwd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



int cmd_pwd(InterpreterRef _Nonnull self, int argc, char** argv)
{
    decl_try_err();
    char* buf = NULL;

    if (argc > 1) {
        printf("%s: unexpected extra arguments\n", argv[0]);
    }

    try_null(buf, malloc(PATH_MAX), ENOMEM);
    try(Process_GetWorkingDirectory(buf, PATH_MAX));
    printf("%s\n", buf);

catch:
    free(buf);
    if (err != EOK) {
        print_error(argv[0], NULL, err);
    }    
    return exit_code(err);
}
