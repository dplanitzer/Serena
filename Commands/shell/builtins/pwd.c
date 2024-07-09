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
#include <clap.h>


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("pwd")
);


int cmd_pwd(InterpreterRef _Nonnull self, int argc, char** argv, char** envp)
{
    decl_try_err();
    char* buf = NULL;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
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
