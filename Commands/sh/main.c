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

static char* skip_whitespace(char* str)
{
    while(*str != '\0') {
        if (!isspace(*str)) {
            break;
        }
        str++;
    }

    return str;
}

static char* skip_non_whitespace(char* str)
{
    while(*str != '\0') {
        if (isspace(*str)) {
            break;
        }
        str++;
    }

    return str;
}


static void sh_cd(char* line)
{
    char* path = skip_whitespace(line);
    char* trailing_ws = skip_non_whitespace(path);
    *trailing_ws = '\0';
    
    if (*path == '\0') {
        printf("Error: expected a path.\n");
    }
    else {
        _chdir(path);
    }
}

static void sh_ls(char* line)
{
    char* path = skip_whitespace(line);
    char* trailing_ws = skip_non_whitespace(path);
    *trailing_ws = '\0';

    if (*path == '\0') {
        path = ".";
    }

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

static void sh_pwd(char* line)
{
    char* trailer = skip_whitespace(line);

    if (*trailer != '\0') {
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


static void parse_and_execute_line(char* line)
{
    line = skip_whitespace(line);

    if (!strncmp(line, "cd", 2)) {
        sh_cd(skip_non_whitespace(line));
    }
    else if (!strncmp(line, "ls", 2)) {
        sh_ls(skip_non_whitespace(line));
    }
    else if (!strncmp(line, "pwd", 3)) {
        sh_pwd(skip_non_whitespace(line));
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

    printf("Apollo Shell v0.1.\n\n");

    _mkdir("/Users");
    _mkdir("/Users/Admin");
    _mkdir("/Users/Tester");


    LineReaderRef pLineReader;
    LineReader_Create(79, 10, ">", &pLineReader);

    while (true) {
        char* line = LineReader_ReadLine(pLineReader);
        puts("");
        parse_and_execute_line(line);
    }
}
