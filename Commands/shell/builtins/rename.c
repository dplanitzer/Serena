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


static errno_t do_rename(const char* _Nonnull oldPath, const char* _Nonnull newPath, const char* _Nonnull proc_name)
{
    decl_try_err();

    err = File_Rename(oldPath, newPath);
    if (err != EOK) {
        print_error(proc_name, oldPath, err);
    }

    return err;
}

int cmd_rename(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_rename(old_path, new_path, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
