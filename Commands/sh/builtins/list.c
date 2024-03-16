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


typedef struct ListContext {
    char*   pathBuffer;

    int     linkCountWidth;
    int     uidWidth;
    int     gidWidth;
    int     sizeWidth;
    int     inodeIdWidth;
} ListContext;
typedef ListContext* ListContextRef;


typedef errno_t (*DirectoryIteratorCallback)(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry);


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

static errno_t calc_dir_entry_format(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry)
{
    decl_try_err();
    FileInfo info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    try(File_GetInfo(self->pathBuffer, &info));

    itoa(info.linkCount, self->pathBuffer, 10);
    self->linkCountWidth = __max(self->linkCountWidth, strlen(self->pathBuffer));
    itoa(info.uid, self->pathBuffer, 10);
    self->uidWidth = __max(self->uidWidth, strlen(self->pathBuffer));
    itoa(info.gid, self->pathBuffer, 10);
    self->gidWidth = __max(self->gidWidth, strlen(self->pathBuffer));
    lltoa(info.size, self->pathBuffer, 10);
    self->sizeWidth = __max(self->sizeWidth, strlen(self->pathBuffer));
    itoa(info.inodeId, self->pathBuffer, 10);
    self->inodeIdWidth = __max(self->inodeIdWidth, strlen(self->pathBuffer));

catch:
    return err;
}

static errno_t print_dir_entry(ListContextRef _Nonnull self, const char* _Nonnull pDirPath, DirectoryEntry* _Nonnull pEntry)
{
    decl_try_err();
    FileInfo info;

    strcpy(self->pathBuffer, pDirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, pEntry->name);

    try(File_GetInfo(self->pathBuffer, &info));

    char tp[11];
    for (int i = 0; i < sizeof(tp); i++) {
        tp[i] = '-';
    }
    if (info.type == kFileType_Directory) {
        tp[0] = 'd';
    }
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsScope_User), &tp[1]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsScope_Group), &tp[4]);
    file_permissions_to_text(FilePermissions_Get(info.permissions, kFilePermissionsScope_Other), &tp[7]);
    tp[10] = '\0';

    printf("%s %*d  %*u %*u  %*lld %*d %s\n",
        tp,
        self->linkCountWidth, info.linkCount,
        self->uidWidth, info.uid,
        self->gidWidth, info.gid,
        self->sizeWidth, info.size,
        self->inodeIdWidth, info.inodeId,
        pEntry->name);
    
catch:
    return err;
}

static errno_t iterate_dir(ListContextRef _Nonnull self, const char* _Nonnull path, DirectoryIteratorCallback _Nonnull cb)
{
    decl_try_err();
    DirectoryEntry dirent;
    ssize_t r;
    int ios;

    try(Directory_Open(path, &ios));
    while (true) {
        try(Directory_Read(ios, &dirent, 1, &r));
        if (r == 0) {
            break;
        }

        try(cb(self, path, &dirent));
    }

catch:
    IOChannel_Close(ios);
    return err;
}

int cmd_list(ShellContextRef _Nonnull pContext, int argc, char** argv)
{
    decl_try_err();
    char* dummy_argv[3] = { argv[0], ".", NULL };
    ListContext ctx = {0};
    bool anyError = false;

    if (argc < 2) {
        argv = dummy_argv;
        argc = 2;
    }

    ctx.pathBuffer = malloc(PATH_MAX);
    if (ctx.pathBuffer == NULL) {
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        const char* path = argv[i];

        if (argc > 2) {
            printf("%s:\n", path);
        }

        errno_t err = iterate_dir(&ctx, path, calc_dir_entry_format);
        if (err == 0) {
            err = iterate_dir(&ctx, path, print_dir_entry);
        }

        if (err != 0) {
            printf("%s: %s.\n", argv[0], strerror(err));
            anyError = true;
        }

        if (i < (argc - 1)) {
            putchar('\n');
        }
    }
    free(ctx.pathBuffer);

    return (anyError) ? EXIT_FAILURE : EXIT_SUCCESS;
}
