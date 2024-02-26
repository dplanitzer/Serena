//
//  list.c
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Interpreter.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <System/_math.h>

typedef errno_t (*DirectoryIteratorCallback)(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry, void* _Nullable pContext);

struct DirectoryEntryFormat {
    int linkCountWidth;
    int uidWidth;
    int gidWidth;
    int sizeWidth;
    int inodeIdWidth;
};


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

static errno_t calc_dir_entry_format(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry, void* _Nullable pContext)
{
    struct DirectoryEntryFormat* fmt = (struct DirectoryEntryFormat*)pContext;
    FileInfo info;
    errno_t err = 0;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    err = File_GetInfo(self->pathBuffer, &info);
    if (err != 0) {
        return err;
    }

    itoa(info.linkCount, self->pathBuffer, 10);
    fmt->linkCountWidth = __max(fmt->linkCountWidth, strlen(self->pathBuffer));
    itoa(info.uid, self->pathBuffer, 10);
    fmt->uidWidth = __max(fmt->uidWidth, strlen(self->pathBuffer));
    itoa(info.gid, self->pathBuffer, 10);
    fmt->gidWidth = __max(fmt->gidWidth, strlen(self->pathBuffer));
    lltoa(info.size, self->pathBuffer, 10);
    fmt->sizeWidth = __max(fmt->sizeWidth, strlen(self->pathBuffer));
    itoa(info.inodeId, self->pathBuffer, 10);
    fmt->inodeIdWidth = __max(fmt->inodeIdWidth, strlen(self->pathBuffer));

    return 0;
}

static errno_t print_dir_entry(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry, void* _Nullable pContext)
{
    struct DirectoryEntryFormat* fmt = (struct DirectoryEntryFormat*)pContext;
    FileInfo info;
    errno_t err = 0;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    err = File_GetInfo(self->pathBuffer, &info);
    if (err != 0) {
        return err;
    }

    char tp[11];
    for (int i = 0; i < sizeof(tp); i++) {
        tp[i] = '-';
    }
    if (info.type == kFileType_Directory) {
        tp[0] = 'd';
    }
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_User), &tp[1]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_Group), &tp[4]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionScope_Other), &tp[7]);
    tp[10] = '\0';

    printf("%s %*ld  %*lu %*lu  %*lld %*ld %s\n",
        tp,
        fmt->linkCountWidth, info.linkCount,
        fmt->uidWidth, info.uid,
        fmt->gidWidth, info.gid,
        fmt->sizeWidth, info.size,
        fmt->inodeIdWidth, info.inodeId,
        pEntry->name);
    
    return 0;
}

static errno_t iterate_dir(InterpreterRef _Nonnull self, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb, void* _Nullable pContext)
{
    int fd;
    errno_t err = 0;

    err = Directory_Open(path, &fd);
    if (err == 0) {
        while (err == 0) {
            DirectoryEntry dirent;
            ssize_t r;

            err = Directory_Read(fd, &dirent, 1, &r);
            if (err != 0 || r == 0) {
                break;
            }

            err = cb(self, path, &dirent, pContext);
            if (err != 0) {
                break;
            }
        }
        IOChannel_Close(fd);
    }
    return err;
}

int cmd_list(InterpreterRef _Nonnull self, int argc, char** argv)
{
    char* dummy_argv[3] = { argv[0], ".", NULL };
    struct DirectoryEntryFormat fmt = {0};
    bool anyError = false;

    if (argc < 2) {
        argv = dummy_argv;
        argc = 2;
    }

    for (int i = 1; i < argc; i++) {
        const char* path = argv[i];

        if (argc > 2) {
            printf("%s:\n", path);
        }

        errno_t err = iterate_dir(self, path, calc_dir_entry_format, &fmt);
        if (err == 0) {
            err = iterate_dir(self, path, print_dir_entry, &fmt);
        }

        if (err != 0) {
            printf("%s: %s.\n", argv[0], strerror(err));
            anyError = true;
        }

        if (i < (argc - 1)) {
            putchar('\n');
        }
    }

    return (anyError) ? EXIT_FAILURE : EXIT_SUCCESS;
}
