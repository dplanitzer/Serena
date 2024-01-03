//
//  main.c
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "LineReader.h"
#include "Parser.h"


static void _chdir(const char* path)
{
    const errno_t err = setcwd(path);

    if (err != 0) {
        printf("Error: %s.\n", strerror(err));
    }
}

static void _mkdir(const char* path)
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


static void sh_cd(ScriptRef _Nonnull pScript)
{
    char* path = (pScript->wordCount > 1) ? pScript->words[1] : "";
    
    if (*path == '\0') {
        printf("Error: expected a path.\n");
    }
    else {
        _chdir(path);
    }
}

static void sh_ls(ScriptRef _Nonnull pScript)
{
    char* path = (pScript->wordCount > 1) ? pScript->words[1] : ".";

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

static void sh_pwd(ScriptRef _Nonnull pScript)
{
    if (pScript->wordCount > 1) {
        printf("Warning: ignored unexpected arguments");
    }

    char buf[128];  //XXX Should be PATH_MAX, however our user stack is not big enough to park it there
    const errno_t err = getcwd(buf, sizeof(buf));

    if (err == 0) {
        printf("%s\n", buf);
    } else {
        printf("Error: %s.\n", strerror(err));
    }
}


static void execute_script(ScriptRef _Nonnull pScript)
{
    if (pScript->wordCount < 1) {
        return;
    }

    const char* cmd = pScript->words[0];

    if (!strcmp(cmd, "cd")) {
        sh_cd(pScript);
    }
    else if (!strcmp(cmd, "ls")) {
        sh_ls(pScript);
    }
    else if (!strcmp(cmd, "pwd")) {
        sh_pwd(pScript);
    }
    else {
        printf("Error: unknown command.\n");
    }
}

////////////////////////////////////////////////////////////////////////////////


void main_closure(int argc, char *argv[])
{
//    assert(open("/dev/console", O_RDONLY) == 0);
//    assert(open("/dev/console", O_WRONLY) == 1);
    int fd0 = open("/dev/console", O_RDONLY);
    int fd1 = open("/dev/console", O_WRONLY);

    printf("Apollo v0.1.\nCopyright 2023, Dietmar Planitzer.\n\n");

    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");


    LineReaderRef pLineReader = NULL;
    ScriptRef pScript = NULL;
    ParserRef pParser = NULL;

    LineReader_Create(79, 10, ">", &pLineReader);
    Script_Create(&pScript);
    Parser_Create(&pParser);

    while (true) {
        char* line = LineReader_ReadLine(pLineReader);
        puts("");
        Parser_Parse(pParser, line, pScript);
        execute_script(pScript);
    }
}
