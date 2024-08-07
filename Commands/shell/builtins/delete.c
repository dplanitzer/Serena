//
//  delete.c
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


static clap_string_array_t paths = {NULL, 0};

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("delete <path ...>"),

    CLAP_REQUIRED_VARARG(&paths, "expected paths of files to delete")
);


static errno_t do_delete_objs(clap_string_array_t* _Nonnull paths, const char* _Nonnull proc_name)
{
    decl_try_err();

    for (size_t i = 0; i < paths->count; i++) {
        const char* path = paths->strings[i];

        err = File_Unlink(path);
        if (err != EOK) {
            print_error(proc_name, path, err);
            break;
        }
    }

    return err;
}

int cmd_delete(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_delete_objs(&paths, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
