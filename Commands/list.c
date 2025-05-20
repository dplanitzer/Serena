//
//  list.c
//  cmds
//
//  Created by Dietmar Planitzer on 4/22/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <clap.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
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

typedef int (*dir_iter_t)(const char* _Nonnull dirPath, const char* _Nonnull entryName);


static int  cur_year;
static int  cur_month;

static int  nlink_w;
static int  uid_w;
static int  gid_w;
static int  size_w;
static int  date_w;

static bool print_all;

static struct tm    date;

static char         buf[BUF_SIZE];
static char         path_buf[PATH_MAX];


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

static int format_inode(const char* _Nonnull path, const char* _Nonnull entryName)
{
    struct stat st;
    
    if (stat(path, &st) != 0) {
        return -1;
    }
    
    itoa(st.st_nlink, buf, 10);
    nlink_w = __max(nlink_w, strlen(buf));
    itoa(st.st_uid, buf, 10);
    uid_w = __max(uid_w, strlen(buf));
    itoa(st.st_gid, buf, 10);
    gid_w = __max(gid_w, strlen(buf));
    lltoa(st.st_size, buf, 10);
    size_w = __max(size_w, strlen(buf));

    // Show time if the date is less than 12 months old; otherwise show date
    localtime_r(&st.st_mtim.tv_sec, &date);
    if (date.tm_year == cur_year || (date.tm_year == cur_year - 1 && date.tm_mon > cur_month)) {
        date_w = TIME_WIDTH;
    }
    else {
        date_w = DATE_WIDTH;
    }

    return 0;
}

static int print_inode(const char* _Nonnull path, const char* _Nonnull entryName)
{
    struct stat st;
    char tc;

    if (stat(path, &st) != 0) {
        return -1;
    }
    
    switch (S_FTYPE(st.st_mode)) {
        case S_IFDEV:   tc = 'h'; break;
        case S_IFDIR:   tc = 'd'; break;
        case S_IFFS:    tc = 'f'; break;
        case S_IFPROC:  tc = 'P'; break;
        case S_IFIFO:   tc = 'p'; break;
        case S_IFLNK:   tc = 'l'; break;
        default:        tc = '-'; break;
    }
    buf[0] = tc;

    for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
        buf[i] = '-';
    }

    file_permissions_to_text(perm_get(st.st_mode, S_ICUSR), &buf[1]);
    file_permissions_to_text(perm_get(st.st_mode, S_ICGRP), &buf[4]);
    file_permissions_to_text(perm_get(st.st_mode, S_ICOTH), &buf[7]);
    buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

    localtime_r(&st.st_mtim.tv_sec, &date);
        
    printf("%s %*d  %*u %*u  %*lld  ",
        buf,
        nlink_w, st.st_nlink,
        uid_w, st.st_uid,
        gid_w, st.st_gid,
        size_w, st.st_size);
    if (date_w == DATE_WIDTH) {
        printf("%s %d %d  ",
            __gc_abbrev_ymon(date.tm_mon + 1),
            date.tm_mday,
            date.tm_year + 1900);
    }
    else {
        printf("%s %d %0.2d:%0.2d  ",
            __gc_abbrev_ymon(date.tm_mon + 1),
            date.tm_mday,
            date.tm_hour,
            date.tm_min);
    }
    fputs(entryName, stdout);
    fputc('\n', stdout);

    return 0;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    __strcat(__strcat(__strcpy(path, dir), "/"), fileName);
}

static int format_dir_entry(const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(path_buf, dirPath, entryName);

    return format_inode(path_buf, entryName);
}

static int print_dir_entry(const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(path_buf, dirPath, entryName);

    return print_inode(path_buf, entryName);
}

static int iterate_dir(DIR* _Nonnull dir, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    errno = 0;

    for (;;) {
        struct dirent* dep = readdir(dir);
        
        if (dep == NULL) {
            break;
        }

        if (print_all || dep->name[0] != '.') {
            if (cb(path, dep->name) != 0) {
                break;
            }
        }
    }

    return (errno == 0) ? 0 : -1;
}

static void list_dir(const char* _Nonnull path)
{
    DIR* dir = opendir(path);

    if (dir) {
        if (iterate_dir(dir, path, format_dir_entry) == 0) {
            rewinddir(dir);
            iterate_dir(dir, path, print_dir_entry);
        }
    
        closedir(dir);
    }
}

static void list_file(const char* _Nonnull path)
{
    if (format_inode(path, path) == 0) {
        print_inode(path, path);
    }
}

static bool is_dir(const char* _Nonnull path)
{
    struct stat st;

    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static const char* default_path[1] = {"."};
static clap_string_array_t paths = {default_path, 1};

CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("list [-a | --all] <path>"),

    CLAP_BOOL('a', "all", &print_all, "Print entries starting with a '.'"),
    CLAP_VARARG(&paths)
);


int main(int argc, char* argv[])
{
    clap_parse(0, params, argc, argv);

    
    errno_t firstErr = EOK;
    const time_t now = time(NULL);

    localtime_r(&now, &date);

    cur_year = date.tm_year;
    cur_month = date.tm_mon;

    for (size_t i = 0; i < paths.count; i++) {
        const char* path = paths.strings[i];

        if (paths.count > 1) {
            printf("%s:\n", path);
        }

        errno = 0;
        const bool isDir = is_dir(path);
        
        if (errno == 0) {
            if (isDir) {
                list_dir(path);
            }
            else {
                list_file(path);
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

    return (firstErr == EOK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
