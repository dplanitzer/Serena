//
//  proc.c
//  sh
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <string.h>
#include <clap.h>
#include <serena/process.h>

#define STR_BUF_SIZE    256

static const char* cmd_id = "";
static const char* pid_str;
static char str_buf[STR_BUF_SIZE];


static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("proc <command> <pid>"),

    CLAP_REQUIRED_COMMAND("name", &cmd_id, "<pid>", "Returns the name of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("cwd", &cmd_id, "<pid>", "Returns the current working directory of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("uid", &cmd_id, "<path>", "Returns the user id of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("gid", &cmd_id, "<path>", "Returns the group id of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("umask", &cmd_id, "<path>", "Returns the umask of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("cmdline", &cmd_id, "<path>", "Returns the command line arguments of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str),

    CLAP_REQUIRED_COMMAND("environ", &cmd_id, "<path>", "Returns the environment of the process 'pid'."),
        CLAP_POSITIONAL_STRING(&pid_str)
);


static int print_com_strings(pid_t pid, const proc_basic_info_t* _Nonnull info, int property)
{
    size_t blksiz;

    switch (property) {
        case PROC_PROP_CMDLINE:     blksiz = info->cmdline_size; break;
        case PROC_PROP_ENVIRON:     blksiz = info->env_size; break;
        default:                    return -1;
    }

    char* buf = malloc(blksiz);
    if (buf == NULL) {
        return -1;
    }

    if (proc_property(pid, property, buf, blksiz) != 0) {
        free(buf);
        return -1;
    }


    char* p = buf;
    for (;;) {
        size_t len = strlen(p);

        if (len == 0) {
            break;
        }

        puts(p);
        p += (len + 1);
    }

    free(buf);
}

static int proc_op(InterpreterRef _Nonnull ip, const char* _Nonnull cmd, pid_t pid)
{
    proc_basic_info_t basic;

    if (proc_info(pid, PROC_INFO_BASIC, &basic) != 0) {
        return EXIT_FAILURE;
    }


    if (!strcmp(cmd, "name")) {
        if (proc_property(pid, PROC_PROP_NAME, str_buf, STR_BUF_SIZE) != 0) {
            return EXIT_FAILURE;
        }

        OpStack_PushCString(ip->opStack, str_buf);
    }
    else if (!strcmp(cmd, "cwd")) {
        if (proc_property(pid, PROC_PROP_CWD, str_buf, STR_BUF_SIZE) != 0) {
            return EXIT_FAILURE;
        }

        OpStack_PushCString(ip->opStack, str_buf);
    }
    else if (!strcmp(cmd, "uid")) {
        OpStack_PushInteger(ip->opStack, basic.uid);
    }
    else if (!strcmp(cmd, "gid")) {
        OpStack_PushInteger(ip->opStack, basic.gid);
    }
    else if (!strcmp(cmd, "umask")) {
        OpStack_PushInteger(ip->opStack, basic.umask);
    }
    else if (!strcmp(cmd, "cmdline")) {
        //XXX this should return an array once this is supported by the shell
        print_com_strings(pid, &basic, PROC_PROP_CMDLINE);
        OpStack_PushVoid(ip->opStack);
    }
    else if (!strcmp(cmd, "environ")) {
        //XXX this should return an array once this is supported by the shell
        print_com_strings(pid, &basic, PROC_PROP_ENVIRON);
        OpStack_PushVoid(ip->opStack);
    }
    else {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int cmd_proc(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    cmd_id = "";
    pid_str = "";
    
    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int r = EXIT_SUCCESS;

    if (!clap_should_exit(status)) {
        r = proc_op(ip, cmd_id, atoi(pid_str));
    }
    else {
        r = clap_exit_code(status);
        OpStack_PushVoid(ip->opStack);
    }
    
    return r;
}
