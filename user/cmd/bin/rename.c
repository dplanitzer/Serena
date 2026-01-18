//
//  rename.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


const char* old_path = "";
const char* new_path = "";

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("rename <old_path> <new_path>"),

    CLAP_REQUIRED_POSITIONAL_STRING(&old_path, "expected a path to an existing file"),
    CLAP_REQUIRED_POSITIONAL_STRING(&new_path, "expected a new location path")
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    if (rename(old_path, new_path) == 0) {
        return EXIT_SUCCESS;
    }
    else {
        clap_error(argv[0], "%s: %s", old_path, strerror(errno));
        return EXIT_FAILURE;
    }
}
