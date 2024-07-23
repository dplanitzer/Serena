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


static errno_t delete_obj(const char* _Nonnull path)
{
    return File_Unlink(path);
}


////////////////////////////////////////////////////////////////////////////////

static clap_string_array_t paths = {NULL, 0};

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("delete <path ...>"),

    CLAP_REQUIRED_VARARG(&paths, "expected paths of files to delete")
);

int cmd_delete(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    decl_try_err();

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }

    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        err = delete_obj(path);
        if (err != EOK) {
            print_error(argv[0], path, err);
            break;
        }
    }

    return exit_code(err);
}
