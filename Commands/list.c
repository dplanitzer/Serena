//
//  list.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/perm.h>
#include <sys/stat.h>
#include <_math.h>

extern const char* __gc_abbrev_ymon(unsigned m);
extern char *__strcpy(char * _Restrict dst, const char * _Restrict src);
extern char *__strcat(char * _Restrict dst, const char * _Restrict src);


// Jan 12  2025
#define DATE_WIDTH (3 + 1 + 2 + 1 + 4)

// Jan 12 13:45
#define TIME_WIDTH (3 + 1 + 2 + 1 + 5)

// Buffer used for various conversions
#define BUF_SIZE    32

// Max length of a permission string
#define PERMISSIONS_STRING_LENGTH  11

typedef struct list_ctx {
    int             currentYear;
    int             currentMonth;

    int             linkCountWidth;
    int             uidWidth;
    int             gidWidth;
    int             sizeWidth;
    int             dateWidth;

    struct Flags {
        unsigned int printAll:1;
        unsigned int reserved:31;
    }               flags;

    struct tm       date;
    char            buf[BUF_SIZE];
    char            pathbuf[PATH_MAX];
} list_ctx_t;


typedef int (*dir_iter_t)(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName);


static void file_permissions_to_text(mode_t perms, char* _Nonnull buf)
{
    if ((perms & S_IREAD) == S_IREAD) {
        buf[0] = 'r';
    }
    if ((perms & S_IWRITE) == S_IWRITE) {
        buf[1] = 'w';
    }
    if ((perms & S_IEXEC) == S_IEXEC) {
        buf[2] = 'x';
    }
}

static int format_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    struct stat info;
    
    if (stat(path, &info) != 0) {
        return -1;
    }
    
    itoa(info.st_nlink, self->buf, 10);
    self->linkCountWidth = __max(self->linkCountWidth, strlen(self->buf));
    itoa(info.st_uid, self->buf, 10);
    self->uidWidth = __max(self->uidWidth, strlen(self->buf));
    itoa(info.st_gid, self->buf, 10);
    self->gidWidth = __max(self->gidWidth, strlen(self->buf));
    lltoa(info.st_size, self->buf, 10);
    self->sizeWidth = __max(self->sizeWidth, strlen(self->buf));

    // Show time if the date is less than 12 months old; otherwise show date
    localtime_r(&info.st_mtim.tv_sec, &self->date);
    if (self->date.tm_year == self->currentYear || (self->date.tm_year == self->currentYear - 1 && self->date.tm_mon > self->currentMonth)) {
        self->dateWidth = TIME_WIDTH;
    }
    else {
        self->dateWidth = DATE_WIDTH;
    }

    return 0;
}

static int print_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    struct stat info;
    char tc;

    if (stat(path, &info) != 0) {
        return -1;
    }
    
    switch (S_FTYPE(info.st_mode)) {
        case S_IFDEV:   tc = 'h'; break;
        case S_IFDIR:   tc = 'd'; break;
        case S_IFFS:    tc = 'f'; break;
        case S_IFPROC:  tc = 'P'; break;
        case S_IFIFO:   tc = 'p'; break;
        case S_IFLNK:   tc = 'l'; break;
        default:        tc = '-'; break;
    }
    self->buf[0] = tc;

    for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
        self->buf[i] = '-';
    }

    file_permissions_to_text(perm_get(info.st_mode, S_ICUSR), &self->buf[1]);
    file_permissions_to_text(perm_get(info.st_mode, S_ICGRP), &self->buf[4]);
    file_permissions_to_text(perm_get(info.st_mode, S_ICOTH), &self->buf[7]);
    self->buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

    localtime_r(&info.st_mtim.tv_sec, &self->date);
        
    printf("%s %*d  %*u %*u  %*lld  ",
        self->buf,
        self->linkCountWidth, info.st_nlink,
        self->uidWidth, info.st_uid,
        self->gidWidth, info.st_gid,
        self->sizeWidth, info.st_size);
    if (self->dateWidth == DATE_WIDTH) {
        printf("%s %d %d  ",
            __gc_abbrev_ymon(self->date.tm_mon + 1),
            self->date.tm_mday,
            self->date.tm_year + 1900);
    }
    else {
        printf("%s %d %0.2d:%0.2d  ",
            __gc_abbrev_ymon(self->date.tm_mon + 1),
            self->date.tm_mday,
            self->date.tm_hour,
            self->date.tm_min);
    }
    fputs(entryName, stdout);
    fputc('\n', stdout);

    return 0;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    __strcat(__strcat(__strcpy(path, dir), "/"), fileName);
}

static int format_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(self->pathbuf, dirPath, entryName);

    return format_inode(self, self->pathbuf, entryName);
}

static int print_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(self->pathbuf, dirPath, entryName);

    return print_inode(self, self->pathbuf, entryName);
}

static int iterate_dir(list_ctx_t* _Nonnull self, DIR* _Nonnull dir, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    errno = 0;

    for (;;) {
        struct dirent* dep = readdir(dir);
        
        if (dep == NULL) {
            break;
        }

        if (self->flags.printAll || dep->name[0] != '.') {
            if (cb(self, path, dep->name) != 0) {
                break;
            }
        }
    }

    return (errno == 0) ? 0 : -1;
}

static void list_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    DIR* dir = opendir(path);

    if (dir) {
        if (iterate_dir(self, dir, path, format_dir_entry) == 0) {
            rewinddir(dir);
            iterate_dir(self, dir, path, print_dir_entry);
        }
    
        closedir(dir);
    }
}

static void list_file(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    if (format_inode(self, path, path) == 0) {
        print_inode(self, path, path);
    }
}

static bool is_dir(const char* _Nonnull path)
{
    struct stat info;

    return (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static const char* default_path[1] = {"."};
static clap_string_array_t paths = {default_path, 1};
static bool is_print_all = false;

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("list [-a | --all] <path>"),

    CLAP_BOOL('a', "all", &is_print_all, "Print entries starting with a '.'"),
    CLAP_VARARG(&paths)
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    errno_t firstErr = EOK;
    const time_t now = time(NULL);

    list_ctx_t* self = calloc(1, sizeof(list_ctx_t));
    if (self == NULL) {
        return EXIT_FAILURE;
    }

    localtime_r(&now, &self->date);

    self->currentYear = self->date.tm_year;
    self->currentMonth = self->date.tm_mon;
    self->flags.printAll = is_print_all;

    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        if (paths.count > 1) {
            printf("%s:\n", path);
        }

        errno = 0;
        const bool isDir = is_dir(path);
        
        if (errno == 0) {
            if (isDir) {
                list_dir(self, path);
            }
            else {
                list_file(self, path);
            }
        }

        if (errno != 0) {
            firstErr = errno;
            clap_error(argv[0], "%s: %s", path, strerror(firstErr));
        }

        if (i < (paths.count - 1)) {
            putchar('\n');
        }
    }
    free(self);

    return (firstErr == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
