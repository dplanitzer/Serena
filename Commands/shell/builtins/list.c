//
//  list.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include "Utilities.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <clap.h>
#include <System/_math.h>


#if defined(__ILP32__)
#define PINID   PRIu32
#elif defined(__LP64__) || defined(__LLP64__)
#define PINID   PRIu64
#else
#error "Unknown data model"
#endif

typedef struct ListContext {
    int     linkCountWidth;
    int     uidWidth;
    int     gidWidth;
    int     sizeWidth;
    int     fsidWidth;
    int     inidWidth;

    struct Flags {
        unsigned int printAll:1;
        unsigned int reserved:31;
    }       flags;

    char    digitBuffer[32];
    char    pathBuffer[PATH_MAX];
} ListContext;
typedef ListContext* ListContextRef;


typedef errno_t (*DirectoryIteratorCallback)(ListContextRef _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName);


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

static errno_t format_inode(ListContextRef _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    FileInfo info;
    const errno_t err = File_GetInfo(path, &info);
    
    if (err == EOK) {
        itoa(info.linkCount, self->digitBuffer, 10);
        self->linkCountWidth = __max(self->linkCountWidth, strlen(self->digitBuffer));
        itoa(info.uid, self->digitBuffer, 10);
        self->uidWidth = __max(self->uidWidth, strlen(self->digitBuffer));
        itoa(info.gid, self->digitBuffer, 10);
        self->gidWidth = __max(self->gidWidth, strlen(self->digitBuffer));
        lltoa(info.size, self->digitBuffer, 10);
        self->sizeWidth = __max(self->sizeWidth, strlen(self->digitBuffer));
        itoa(info.filesystemId, self->digitBuffer, 10);
        self->fsidWidth = __max(self->fsidWidth, strlen(self->digitBuffer));
        itoa(info.inodeId, self->digitBuffer, 10);
        self->inidWidth = __max(self->inidWidth, strlen(self->digitBuffer));
    }
    return err;
}

static errno_t print_inode(ListContextRef _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    FileInfo info;
    const errno_t err = File_GetInfo(path, &info);
    
    if (err == EOK) {
        char tp[11];

        for (int i = 0; i < sizeof(tp); i++) {
            tp[i] = '-';
        }
        if (info.type == kFileType_Directory) {
            tp[0] = 'd';
        }
        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_User), &tp[1]);
        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Group), &tp[4]);
        file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsClass_Other), &tp[7]);
        tp[10] = '\0';

        printf("%s %*d  %*u %*u  %*lld %*x`%*"PINID" %s\n",
            tp,
            self->linkCountWidth, info.linkCount,
            self->uidWidth, info.uid,
            self->gidWidth, info.gid,
            self->sizeWidth, info.size,
            self->fsidWidth, info.filesystemId,
            self->inidWidth, info.inodeId,
            entryName);
    }
    return err;
}


static errno_t format_dir_entry(ListContextRef _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    strcpy(self->pathBuffer, dirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, entryName);

    return format_inode(self, self->pathBuffer, entryName);
}

static errno_t print_dir_entry(ListContextRef _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    strcpy(self->pathBuffer, dirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, entryName);

    return print_inode(self, self->pathBuffer, entryName);
}

static errno_t iterate_dir(ListContextRef _Nonnull self, int dp, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb)
{
    decl_try_err();
    DirectoryEntry dirent;
    ssize_t r;

    while (true) {
        err = Directory_Read(dp, &dirent, 1, &r);
        if (err != EOK || r == 0) {
            break;
        }

        if (!self->flags.printAll && dirent.name[0] == '.') {
            continue;
        }

        err = cb(self, path, dirent.name);
        if (err != EOK) {
            break;
        }
    }

    return err;
}

static errno_t list_dir(ListContextRef _Nonnull self, const char* _Nonnull path)
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

static errno_t list_file(ListContextRef _Nonnull self, const char* _Nonnull path)
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
    ListContext* ctx = calloc(1, sizeof(ListContext));

    if (ctx == NULL) {
        return ENOMEM;
    }

    ctx->flags.printAll = isPrintAll;

    for (size_t i = 0; i < paths->count; i++) {
        const char* path = paths->strings[i];

        if (paths->count > 1) {
            printf("%s:\n", path);
        }

        if (is_dir(path)) {
            err = list_dir(ctx, path);
        }
        else {
            err = list_file(ctx, path);
        }
        if (err != EOK) {
            firstErr = err;
            print_error(proc_name, path, err);
        }

        if (i < (paths->count - 1)) {
            putchar('\n');
        }
    }
    free(ctx);

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
