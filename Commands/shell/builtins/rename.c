//
//  rename.c
//  sh
//
//  Created by Dietmar Planitzer on 5/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <clap.h>


static const char* old_path;
static const char* new_path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("rename <old_path> <new_path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&old_path, "expected a path to an existing file"),
    CLAP_REQUIRED_POSITIONAL_STRING(&new_path, "expected a new location path")
);


int cmd_rename(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }

    err = File_Rename(old_path, new_path);
    if (err != EOK) {
        print_error(argv[0], old_path, err);
    }

    return exit_code(err);
}
