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
#include <unistd.h>
#include <sys/stat.h>
#include <clap.h>

static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("pushcd <directory>"),

    CLAP_POSITIONAL_STRING(&path)
);


static int do_pushcd(InterpreterRef _Nonnull ip, const char* path, const char* proc_name)
{
    CDEntry* entry = NULL;
    char* buf = malloc(PATH_MAX);
    
    getcwd(buf, PATH_MAX);
    entry = calloc(1, sizeof(CDEntry));
    entry->path = strdup(buf);

    if (*path != '\0') {
        if (chdir(path) != 0) {
            free(entry->path);
            free(entry);
            print_error(proc_name, path, errno);
            return EXIT_FAILURE;
        }
    }

    if (ip->cdStackTos) {
        entry->prev = ip->cdStackTos;
    }
    ip->cdStackTos = entry;

    free(buf);

    return EXIT_SUCCESS;
}

int cmd_pushcd(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    path = "";

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = do_pushcd(ip, path, argv[0]);
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
