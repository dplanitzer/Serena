//
//  pushcd.c
//  sh
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clap.h>


static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("pushcd <directory>"),

    CLAP_POSITIONAL_STRING(&path)
);


static errno_t do_pushcd(InterpreterRef _Nonnull ip, const char* path, const char* proc_name)
{
    decl_try_err();
    CDEntry* entry = NULL;
    char* buf = NULL;
    
    try_null(buf, malloc(PATH_MAX), ENOMEM);
    try(os_getcwd(buf, PATH_MAX));
    try_null(entry, calloc(1, sizeof(CDEntry)), ENOMEM);
    try_null(entry->path, strdup(buf), ENOMEM);

    if (*path != '\0') {
        try(os_setcwd(path));
    }

    if (ip->cdStackTos) {
        entry->prev = ip->cdStackTos;
    }
    ip->cdStackTos = entry;

    return EOK;

catch:
    print_error(proc_name, path, err);

    free(buf);
    if (entry) {
        free(entry->path);
    }
    free(entry);

    return err;
}

int cmd_pushcd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    path = "";

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_pushcd(ip, path, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
