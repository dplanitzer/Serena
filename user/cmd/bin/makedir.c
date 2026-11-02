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
#include <ext/path.h>
#include <serena/directory.h>
#include <serena/file.h>


static int create_directory_recursively(char* _Nonnull path, fs_perms_t fsperms)
{
    path_iter_t it = PATH_ITER_INITIALIZER;
    fs_attr_t attr;

    // Iterate through the path string
    while (!pathnext(&it, path)) {
        char* p = (char*)it.end;

        if (*p == '/') {
            *p = '\0';

            if (fs_create_directory(NULL, path, fsperms) != 0) {
                // Stop if the intermediate "directory" is actually a file
                if (errno == EEXIST && fs_attr(NULL, path, &attr) == 0 && attr.file_type != FS_FTYPE_DIR) {
                    errno = ENOTDIR;
                    return -1;
                }
            }

            *p = '/';
        }
    }

    // Create the final target directory
    return fs_create_directory(NULL, path, fsperms);
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
    const fs_perms_t fsperms = fs_perms_from_octal(0755);
    int r = 0;

    clap_parse(0, params, argc, argv);

    
    for (size_t i = 0; i < paths.count; i++) {
        char* path = (char*)paths.strings[i];

        if (strlen(path) < PATH_MAX) {
            if (should_create_parents) {
                r = create_directory_recursively(path, fsperms);
            }
            else {
                r = fs_create_directory(NULL, path, fsperms);
            }
        }
        else {
            errno = ENAMETOOLONG;
            r = -1;
        }


        if (r != 0) {
            clap_error(argv[0], "%s: %s", path, strerror(errno));
            break;
        }
    }

    return (r == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
