//
//  list.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define _POSIX_SOURCE
#include <time.h>
#include <clap.h>
#include <System/_math.h>

extern const char* __gc_abbrev_ymon(unsigned m);
extern char *__strcpy(char * _Restrict dst, const char * _Restrict src);
extern char *__strcat(char * _Restrict dst, const char * _Restrict src);


// Jan 12  2025
#define DATE_WIDTH (3 + 1 + 2 + 1 + 4)

// Jan 12 13:45
#define TIME_WIDTH (3 + 1 + 2 + 1 + 5)

// Buffer holding directory entries
#define DIRBUF_SIZE 12

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

    DirectoryEntry  dirbuf[DIRBUF_SIZE];
} list_ctx_t;


typedef errno_t (*dir_iter_t)(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName);


static void file_permissions_to_text(FilePermissions perms, char* _Nonnull buf)
{
    if ((perms & kFilePermission_Read) == kFilePermission_Read) {
        buf[0] = 'r';
    }
    if ((perms & kFilePermission_Write) == kFilePermission_Write) {
        buf[1] = 'w';
    }
    if ((perms & kFilePermission_Execute) == kFilePermission_Execute) {
        buf[2] = 'x';
    }
}

static errno_t format_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    FileInfo info;
    const errno_t err = File_GetInfo(path, &info);
    
    if (err == EOK) {
        itoa(info.linkCount, self->buf, 10);
        self->linkCountWidth = __max(self->linkCountWidth, strlen(self->buf));
        itoa(info.uid, self->buf, 10);
        self->uidWidth = __max(self->uidWidth, strlen(self->buf));
        itoa(info.gid, self->buf, 10);
        self->gidWidth = __max(self->gidWidth, strlen(self->buf));
        lltoa(info.size, self->buf, 10);
        self->sizeWidth = __max(self->sizeWidth, strlen(self->buf));

        // Show time if the date is less than 12 months old; otherwise show date
        localtime_r(&info.modificationTime.tv_sec, &self->date);
        if (self->date.tm_year == self->currentYear || (self->date.tm_year == self->currentYear - 1 && self->date.tm_mon > self->currentMonth)) {
            self->dateWidth = TIME_WIDTH;
        }
        else {
            self->dateWidth = DATE_WIDTH;
        }
    }
    return err;
}

static errno_t print_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    FileInfo info;
    const errno_t err = File_GetInfo(path, &info);
    
    if (err == EOK) {
        char tc;

        switch (info.type) {
            case kFileType_Device:              tc = 'h'; break;
            case kFileType_Directory:           tc = 'd'; break;
            case kFileType_Pipe:                tc = 'p'; break;
            case kFileType_SymbolicLink:        tc = 'l'; break;
            default:                            tc = '-'; break;
        }
        self->buf[0] = tc;

        for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
            self->buf[i] = '-';
        }

        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_User), &self->buf[1]);
        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Group), &self->buf[4]);
        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Other), &self->buf[7]);
        self->buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

        localtime_r(&info.modificationTime.tv_sec, &self->date);
        
        printf("%s %*d  %*u %*u  %*lld  ",
            self->buf,
            self->linkCountWidth, info.linkCount,
            self->uidWidth, info.uid,
            self->gidWidth, info.gid,
            self->sizeWidth, info.size);
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
    }
    return err;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    __strcat(__strcat(__strcpy(path, dir), "/"), fileName);
}

static errno_t format_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(self->pathbuf, dirPath, entryName);

    return format_inode(self, self->pathbuf, entryName);
}

static errno_t print_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    concat_path(self->pathbuf, dirPath, entryName);

    return print_inode(self, self->pathbuf, entryName);
}

static errno_t iterate_dir(list_ctx_t* _Nonnull self, int dp, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    decl_try_err();
    ssize_t nBytesRead;

    while (err == EOK) {
        err = Directory_Read(dp, self->dirbuf, sizeof(self->dirbuf), &nBytesRead);
        if (err != EOK || nBytesRead == 0) {
            break;
        }

        const DirectoryEntry* dep = self->dirbuf;
        
        while (nBytesRead > 0) {
            if (self->flags.printAll || dep->name[0] != '.') {
                err = cb(self, path, dep->name);
                if (err != EOK) {
                    break;
                }
            }

            nBytesRead -= sizeof(DirectoryEntry);
            dep++;
        }
    }

    return err;
}

static errno_t list_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    int dp;

    try(Directory_Open(path, &dp));
    try(iterate_dir(self, dp, path, format_dir_entry));
    try(Directory_Rewind(dp));
    try(iterate_dir(self, dp, path, print_dir_entry));

catch:
    IOChannel_Close(dp);
    return err;
}

static errno_t list_file(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();

    err = format_inode(self, path, path);
    if (err == EOK) {
        err = print_inode(self, path, path);
    }

    return err;
}

static bool is_dir(const char* _Nonnull path)
{
    FileInfo info;

    return (File_GetInfo(path, &info) == EOK && info.type == kFileType_Directory) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static const char* default_path[1] = {"."};
static clap_string_array_t paths = {default_path, 1};
static bool is_print_all = false;

static CLAP_DECL(params,
    CLAP_VERSION("1.0"),
    CLAP_HELP(),
    CLAP_USAGE("list [-a | --all] <path>"),

    CLAP_BOOL('a', "all", &is_print_all, "Print entries starting with a '.'"),
    CLAP_VARARG(&paths)
);


static errno_t do_list(clap_string_array_t* _Nonnull paths, bool isPrintAll, const char* _Nonnull proc_name)
{
    decl_try_err();
    errno_t firstErr = EOK;
    const time_t now = time(NULL);

    list_ctx_t* self = calloc(1, sizeof(list_ctx_t));
    if (self == NULL) {
        return ENOMEM;
    }

    localtime_r(&now, &self->date);

    self->currentYear = self->date.tm_year;
    self->currentMonth = self->date.tm_mon;
    self->flags.printAll = isPrintAll;

    for (size_t i = 0; i < paths->count; i++) {
        const char* path = paths->strings[i];

        if (paths->count > 1) {
            printf("%s:\n", path);
        }

        if (is_dir(path)) {
            err = list_dir(self, path);
        }
        else {
            err = list_file(self, path);
        }
        if (err != EOK) {
            firstErr = err;
            print_error(proc_name, path, err);
        }

        if (i < (paths->count - 1)) {
            putchar('\n');
        }
    }
    free(self);

    return firstErr;
}

int cmd_list(InterpreterRef _Nonnull ip, int argc, char** argv, char** envp)
{
    paths.strings = default_path;
    paths.count = 1;
    is_print_all = false;

    const int status = clap_parse(clap_option_no_exit, params, argc, argv);
    int exitCode;

    if (!clap_should_exit(status)) {
        exitCode = exit_code(do_list(&paths, is_print_all, argv[0]));
    }
    else {
        exitCode = clap_exit_code(status);
    }

    OpStack_PushVoid(ip->opStack);
    return exitCode;
}
