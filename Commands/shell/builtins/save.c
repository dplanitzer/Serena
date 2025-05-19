//
//  save.c
//  sh
//
//  Created by Dietmar Planitzer on 8/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clap.h>


static clap_string_array_t parts = {NULL, 0};
static bool is_append = false;
static bool is_raw = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("save [-a|--append] [-r|--raw] <text> to <path>"),

    CLAP_BOOL('a', "append", &is_append, "append to the end of the file"),
    CLAP_BOOL('r', "raw", &is_raw, "save file as a raw binary"),
    CLAP_REQUIRED_VARARG(&parts, "")
);


static int do_save(const char* _Nonnull text, const char* _Nonnull path, const char* _Nonnull proc_name)
{
    const size_t textLen = strlen(text);
    char mode[3] = {'w', '\0', '\0'};

    if (is_append) {
        mode[0] = 'a';
    }
    if (is_raw) {
        mode[1] = 'b';
    }

    FILE* fp = fopen(path, mode);
    if (fp) {
        fwrite(text, textLen, 1, fp);
        fclose(fp);
    }

    if (fp == NULL || (fp && ferror(fp))) {
        print_error(proc_name, path, errno);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

int cmd_save(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        // save "test" to my_file.txt
        if (parts.count != 3 || strcmp(parts.strings[1], "to")) {
            clap_error(argv[0], "expected 'save <text> to <path>");
            exitCode = EXIT_FAILURE;
        }
        else {
            exitCode = do_save(parts.strings[0], parts.strings[2], argv[0]);
        }
    }
    else {
        exitCode = clap_exit_code(status);
    }    
    
    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
