//
//  Interpreter.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void _chdir(const char* path)
{
    const errno_t err = setcwd(path);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

static void _remove(const char* path)
{
    const errno_t err = unlink(path);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

void _mkdir(const char* path)
{
    const errno_t err = mkdir(path, 0777);
    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

static int _opendir(const char* path)
{
    int fd = -1;
    const errno_t err = opendir(path, &fd);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
    return fd;
}

static size_t _read(int fd, void* buffer, size_t nbytes)
{
    ssize_t r = read(fd, buffer, nbytes);

    if (r < 0) {
        printf("Error: %s.\n", strerror(-r));
    }
    return (size_t)r;
}

static size_t _write(int fd, const void* buffer, size_t nbytes)
{
    ssize_t r = write(fd, buffer, nbytes);

    if (r < 0) {
        printf("Error: %s.\n", strerror(-r));
    }
    return (size_t)r;
}

static void _close(int fd)
{
    const errno_t err = close(fd);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}


////////////////////////////////////////////////////////////////////////////////

errno_t Interpreter_Create(InterpreterRef _Nullable * _Nonnull pOutInterpreter)
{
    InterpreterRef self = (InterpreterRef)calloc(1, sizeof(Interpreter));

    if (self == NULL) {
        *pOutInterpreter = NULL;
        return ENOMEM;
    }

    *pOutInterpreter = self;
    return 0;
}

void Interpreter_Destroy(InterpreterRef _Nullable self)
{
    if (self) {
        free(self);
    }
}

////////////////////////////////////////////////////////////////////////////////


static void sh_cd(StatementRef _Nonnull pStatement)
{
    char* path = (pStatement->wordCount > 1) ? pStatement->words[1] : "";
    
    if (*path == '\0') {
        printf("Error: expected a path.\n");
    }
    else {
        _chdir(path);
    }
}

static void sh_ls(StatementRef _Nonnull pStatement)
{
    char* path = (pStatement->wordCount > 1) ? pStatement->words[1] : ".";

    const int fd = _opendir(path);
    if (fd != -1) {
        while (true) {
            struct _directory_entry_t dirent;
            const ssize_t r = _read(fd, &dirent, sizeof(dirent));
        
            if (r == 0) {
                break;
            }

            printf("%ld:\t\"%s\"\n", dirent.inodeId, dirent.name);
        }
        _close(fd);
    }
}

static void sh_pwd(StatementRef _Nonnull pStatement)
{
    if (pStatement->wordCount > 1) {
        printf("Warning: ignored unexpected arguments\n");
    }

    char buf[128];  //XXX Should be PATH_MAX, however our user stack is not big enough to park it there
    const errno_t err = getcwd(buf, sizeof(buf));

    if (err == 0) {
        printf("%s\n", buf);
    } else {
        printf("Error: %s.\n", strerror(err));
    }
}

static void sh_mkdir(StatementRef _Nonnull pStatement)
{
    char* path = (pStatement->wordCount > 1) ? pStatement->words[1] : "";

    if (*path == '\0') {
        printf("Error: expected a path\n");
        return;
    }

    _mkdir(path);
}

static void sh_rm(StatementRef _Nonnull pStatement)
{
    char* path = (pStatement->wordCount > 1) ? pStatement->words[1] : "";

    if (*path == '\0') {
        printf("Error: expected a path");
        return;
    }

    _remove(path);
}


static void execute_statement(StatementRef _Nonnull pStatement)
{
    if (pStatement->wordCount < 1) {
        return;
    }

    const char* cmd = pStatement->words[0];

    if (!strcmp(cmd, "cd")) {
        sh_cd(pStatement);
    }
    else if (!strcmp(cmd, "ls")) {
        sh_ls(pStatement);
    }
    else if (!strcmp(cmd, "pwd")) {
        sh_pwd(pStatement);
    }
    else if (!strcmp(cmd, "mkdir")) {
        sh_mkdir(pStatement);
    }
    else if (!strcmp(cmd, "rm")) {
        sh_rm(pStatement);
    }
    else {
        printf("Error: unknown command.\n");
    }
}

// Interprets 'pScript' and executes all its statements.
void Interpreter_Execute(InterpreterRef _Nonnull self, ScriptRef _Nonnull pScript)
{
    StatementRef st = pScript->statements;

    while (st) {
        execute_statement(st);
        st = st->next;
    }
}
