//
//  delete.c
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


static clap_string_array_t paths = {NULL, 0};

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("delete <path ...>"),

    CLAP_REQUIRED_VARARG(&paths, "expected paths to files to delete")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        if (remove(path) != 0) {
            clap_error(argv[0], "%s: %s", path, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
