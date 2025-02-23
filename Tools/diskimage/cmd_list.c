//
//  cmd_list.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <System/_math.h>
#include <System/Directory.h>


#if defined(__ILP32__)
#define PINID   PRIu32
#elif defined(__LP64__) || defined(__LLP64__)
#define PINID   PRIu64
#else
#error "Unknown data model"
#endif

#ifdef _WIN32
#define lltoa _i64toa
#endif


typedef struct list_ctx {
    DiskControllerRef _Nonnull  dc;
    FileManagerRef _Nonnull     fm;
    int                         linkCountWidth;
    int                         uidWidth;
    int                         gidWidth;
    int                         sizeWidth;
    int                         inodeIdWidth;

    struct Flags {
        unsigned int printAll:1;
        unsigned int reserved:31;
    }                           flags;

    char                        digitBuffer[32];
    char                        pathBuffer[PATH_MAX];
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
    const errno_t err = FileManager_GetFileInfo(self->fm, path, &info);
    
    if (err == EOK) {
        itoa(info.linkCount, self->digitBuffer, 10);
        self->linkCountWidth = __max(self->linkCountWidth, strlen(self->digitBuffer));
        itoa(info.uid, self->digitBuffer, 10);
        self->uidWidth = __max(self->uidWidth, strlen(self->digitBuffer));
        itoa(info.gid, self->digitBuffer, 10);
        self->gidWidth = __max(self->gidWidth, strlen(self->digitBuffer));
        lltoa(info.size, self->digitBuffer, 10);
        self->sizeWidth = __max(self->sizeWidth, strlen(self->digitBuffer));
        itoa(info.inodeId, self->digitBuffer, 10);
        self->inodeIdWidth = __max(self->inodeIdWidth, strlen(self->digitBuffer));
    }
    return err;
}

static errno_t print_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    FileInfo info;
    const errno_t err = FileManager_GetFileInfo(self->fm, path, &info);
    
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

        printf("%s %*d  %*u %*u  %*lld %*" PINID " %s\n",
            tp,
            self->linkCountWidth, info.linkCount,
            self->uidWidth, info.uid,
            self->gidWidth, info.gid,
            self->sizeWidth, info.size,
            self->inodeIdWidth, info.inodeId,
            entryName);
    }
    return err;
}


static errno_t format_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    strcpy(self->pathBuffer, dirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, entryName);

    return format_inode(self, self->pathBuffer, entryName);
}

static errno_t print_dir_entry(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName)
{
    strcpy(self->pathBuffer, dirPath);
    strcat(self->pathBuffer, "/");
    strcat(self->pathBuffer, entryName);

    return print_inode(self, self->pathBuffer, entryName);
}

static errno_t iterate_dir(list_ctx_t* _Nonnull self, IOChannelRef _Nonnull chan, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    decl_try_err();
    DirectoryEntry dirent;
    ssize_t r;

    while (true) {
        err = IOChannel_Read(chan, &dirent, sizeof(DirectoryEntry), &r);
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

static errno_t list_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    decl_try_err();
    IOChannelRef chan = NULL;

    try(FileManager_OpenDirectory(self->fm, path, &chan));
    try(iterate_dir(self, chan, path, format_dir_entry));
    try(IOChannel_Seek(chan, 0ll, NULL, kSeek_Set));
    try(iterate_dir(self, chan, path, print_dir_entry));

catch:
    IOChannel_Release(chan);
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

static bool is_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    FileInfo info;

    return (FileManager_GetFileInfo(self->fm, path, &info) == EOK && info.type == kFileType_Directory) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static errno_t do_list(DiskControllerRef dc, const char* _Nonnull path, bool isPrintAll)
{
    decl_try_err();

    list_ctx_t* ctx = calloc(1, sizeof(list_ctx_t));
    if (ctx == NULL) {
        return ENOMEM;
    }

    ctx->dc = dc;
    ctx->fm = &dc->fm;
    ctx->flags.printAll = isPrintAll;

    if (is_dir(ctx, path)) {
        err = list_dir(ctx, path);
    }
    else {
        err = list_file(ctx, path);
    }

    free(ctx);

    return err;
}

errno_t cmd_list(const char* _Nonnull path, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef dc;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &dc));
    err = do_list(dc, path, false);

catch:
    DiskController_Destroy(dc);
    return err;
}
