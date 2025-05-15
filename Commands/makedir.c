//
//  makedir.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


static int _create_directory_recursively(char* _Nonnull path, FilePermissions permissions)
{
    char* p = path;

    while (*p == '/') {
        p++;
    }

    do {
        char* ps = strchr(p, '/');

        if (ps) { *ps = '\0'; }

        const int r = mkdir(path, permissions);
        if (ps) { *ps = '/'; }

        if (r != 0 && errno != EEXIST) {
            return -1;
        }
        if (ps == NULL) {
            break;
        }

        p = ps;
        while (*p == '/') {
            p++;
        }
    } while(*p != '\0');

    return 0;
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
static int create_directory_recursively(char* _Nonnull path, FilePermissions permissions)
{
    int i = 0;

    while (i < 16) {
        if (_create_directory_recursively(path, permissions) == 0) {
            break;
        }
        
        if (errno != ENOENT) {
            return -1;
        }

        i++;
    }
    
    return 0;
}


////////////////////////////////////////////////////////////////////////////////

static clap_string_array_t paths = {NULL, 0};
static bool should_create_parents = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("makedir [-p | --parents] <path>"),

    CLAP_BOOL('p', "parents", &should_create_parents, "Create missing parent directories"),
    CLAP_REQUIRED_VARARG(&paths, "expected paths of directories to create")
);


int main(int argc, char* argv[])
{
    const FilePermissions permissions = FilePermissions_MakeFromOctal(0755);
    int r = 0;

    clap_parse(0, params, argc, argv);

    
    for (size_t i = 0; i < paths.count; i++) {
        char* path = (char*)paths.strings[i];

        r = mkdir(path, permissions);
        if (r != 0) {
            if (errno == ENOENT && should_create_parents) {
                r = create_directory_recursively(path, permissions);
            }

            if (r != 0) {
                clap_error(argv[0], "%s: %s", path, strerror(errno));
            }
        }

        if (r != 0) {
            break;
        }
    }

    return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
