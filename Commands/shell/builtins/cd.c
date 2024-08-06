//
//  cd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("cd <directory>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a path to a directory")
);


int cmd_cd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }    
    
    err = Process_SetWorkingDirectory(path);
    if (err != EOK) {
        print_error(argv[0], path, err);
    }

    return exit_code(err);
}
