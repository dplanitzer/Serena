//
//  cd.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "cmdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


const char* path;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("cd <directory>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&path, "expected a path to a directory")
);


int cmd_cd(ShellContextRef _Nonnull pContext, int argc, char** argv)
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
