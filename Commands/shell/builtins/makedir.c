//
//  makedir.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clap.h>


static errno_t _create_directory_recursively(char* _Nonnull path, FilePermissions permissions)
{
    decl_try_err();
    char* p = path;

    while (*p == '/') {
        p++;
    }

    do {
        char* ps = strchr(p, '/');

        if (ps) { *ps = '\0'; }

        err = Directory_Create(path, permissions);
        if (ps) { *ps = '/'; }

        if (err != EOK && err != EEXIST || ps == NULL) {
            break;
        }

        p = ps;
        while (*p == '/') {
            p++;
        }
    } while(*p != '\0');

    return err;
}

// Iterate the path components from the root on down and try creating the
// corresponding directory. If it fails with EEXIST then we know that this
// directory already exists. Any other error is treated as fatal. If it worked
// then continue until we hit the end of the path.
// Note that we may find ourselves racing with another process that is busy
// deleting one of the path components we thought existed. Ie we try to do
// a create directory on path component X that comes back with EEXIST. We now
// move on to the child X/Y and try the create directory there. However this
// may now come back with ENOENT because X was empty and it got deleted by
// another process. We simply start over again from the root of our path in
// this case.
static errno_t create_directory_recursively(char* _Nonnull path, FilePermissions permissions)
{
    decl_try_err();
    int i = 0;

    while (i < 16) {
        err = _create_directory_recursively(path, permissions);
        if (err == EOK || err != ENOENT) {
            break;
        }

        i++;
    }
    
    return err;
}


////////////////////////////////////////////////////////////////////////////////

static clap_string_array_t paths = {NULL, 0};
static bool should_create_parents = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("makedir [-p | --parents] <path>"),

    CLAP_BOOL('p', "parents", &should_create_parents, "Create missing parent directories"),
    CLAP_REQUIRED_VARARG(&paths, "expected paths of directories to create")
);


int cmd_makedir(ShellContextRef _Nonnull pContext, int argc, char** argv, char** envp)
{
    decl_try_err();
    FilePermissions permissions = FilePermissions_MakeFromOctal(0755);

    should_create_parents = false;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    if (clap_should_exit(status)) {
        return clap_exit_code(status);
    }

    for (size_t i = 0; i < paths.count; i++) {
        char* path = (char*)paths.strings[i];

        err = Directory_Create(path, permissions);
        if (err != EOK) {
            if (err == ENOENT && should_create_parents) {
                err = create_directory_recursively(path, permissions);
            }

            if (err != EOK) {
                print_error(argv[0], path, err);
            }
        }
    }

    return exit_code(err);
}
