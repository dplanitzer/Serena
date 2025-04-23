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
//#include <System/Types.h>


const char* old_path = "";
const char* new_path = "";

CLAP_DECL(params,
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
        clap_error(proc_name, "%s: %s", oldPath, strerror(err));
    }

    return err;
}

int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);
    
    return do_rename(old_path, new_path, argv[0]) == EOK ? EXIT_SUCCESS : EXIT_FAILURE;
}
