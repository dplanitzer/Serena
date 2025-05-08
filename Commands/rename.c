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
#include <stdlib.h>
#include <string.h>
#include <System/Error.h>
#include <System/File.h>


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
    decl_try_err();

    clap_parse(0, params, argc, argv);

    
    err = os_rename(old_path, new_path);
    if (err != EOK) {
        clap_error(argv[0], "%s: %s", old_path, strerror(err));
    }

    return (err == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
