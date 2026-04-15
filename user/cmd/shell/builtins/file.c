//
//  file.c
//  sh
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <string.h>
#include <clap.h>
#include <serena/file.h>


static const char* cmd_id = "";
static const char* path;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("file <command> <path>"),

    CLAP_REQUIRED_COMMAND("size", &cmd_id, "<path>", "Returns the size of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path),

    CLAP_REQUIRED_COMMAND("uid", &cmd_id, "<path>", "Returns the user id of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path),

    CLAP_REQUIRED_COMMAND("gid", &cmd_id, "<path>", "Returns the group id of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path),

    CLAP_REQUIRED_COMMAND("permissions", &cmd_id, "<path>", "Returns the permissions of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path),

    CLAP_REQUIRED_COMMAND("type", &cmd_id, "<path>", "Returns the file type of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path),

    CLAP_REQUIRED_COMMAND("fsid", &cmd_id, "<path>", "Returns the filesystem id of the file at 'path'."),
        CLAP_POSITIONAL_STRING(&path)
);


int file_op(InterpreterRef _Nonnull ip, const char* _Nonnull cmd, const char* _Nonnull path)
{
    fs_attr_t attr;

    if (fs_attr(NULL, path, &attr) != 0) {
        return EXIT_FAILURE;
    }


    if (!strcmp(cmd, "size")) {
        OpStack_PushInteger(ip->opStack, (int32_t)attr.size);
    }
    else if (!strcmp(cmd, "uid")) {
        OpStack_PushInteger(ip->opStack, attr.uid);
    }
    else if (!strcmp(cmd, "gid")) {
        OpStack_PushInteger(ip->opStack, attr.gid);
    }
    else if (!strcmp(cmd, "permissions")) {
        OpStack_PushInteger(ip->opStack, attr.permissions);
    }
    else if (!strcmp(cmd, "type")) {
        OpStack_PushInteger(ip->opStack, attr.file_type);
    }
    else if (!strcmp(cmd, "fsid")) {
        OpStack_PushInteger(ip->opStack, attr.fsid);
    }
    else {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int cmd_file(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    cmd_id = "";
    path = "";
    
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int r = EXIT_SUCCESS;

    if (!clap_should_exit(status)) {
        r = file_op(ip, cmd_id, path);
    }
    else {
        r = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return r;
}
