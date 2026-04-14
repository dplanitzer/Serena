//
//  cmd_list.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "FSManager.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ext/perm.h>
#include <ext/stdlib.h>
#include <filesystem/IOChannel.h>
#include <kpi/directory.h>

#define PINID   PRIu32
//#define PINID   PRIu64


// Buffer holding directory entries
#define ENTBUF_COUNT 16

// Buffer used for various conversions
#define BUF_SIZE    32

// Max length of a permission string
#define PERMISSIONS_STRING_LENGTH  11


typedef struct list_ctx {
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

    char                        buf[BUF_SIZE];
    char                        pathbuf[PATH_MAX];

    dir_entry_t               dirbuf[ENTBUF_COUNT];
} list_ctx_t;


typedef errno_t (*dir_iter_t)(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName);


static void file_permissions_to_text(fs_perms_t perms, char* _Nonnull buf)
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

static errno_t format_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;
    const errno_t err = FileManager_GetAttributes(self->fm, path, &attr);
    
    if (err == EOK) {
        itoa(attr.nlink, self->buf, 10);
        self->linkCountWidth = __max(self->linkCountWidth, strlen(self->buf));
        itoa(attr.uid, self->buf, 10);
        self->uidWidth = __max(self->uidWidth, strlen(self->buf));
        itoa(attr.gid, self->buf, 10);
        self->gidWidth = __max(self->gidWidth, strlen(self->buf));
        lltoa(attr.size, self->buf, 10);
        self->sizeWidth = __max(self->sizeWidth, strlen(self->buf));
        itoa(attr.ino, self->buf, 10);
        self->inodeIdWidth = __max(self->inodeIdWidth, strlen(self->buf));
    }
    return err;
}

static errno_t print_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;
    const errno_t err = FileManager_GetAttributes(self->fm, path, &attr);
    
    if (err == EOK) {
        char tc;

        switch (S_FTYPE(attr.mode)) {
            case S_IFDEV:   tc = 'h'; break;
            case S_IFDIR:   tc = 'd'; break;
            case S_IFPROC:  tc = 'P'; break;
            case S_IFIFO:   tc = 'p'; break;
            case S_IFLNK:   tc = 'l'; break;
            default:        tc = '-'; break;
        }
        self->buf[0] = tc;

        for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
            self->buf[i] = '-';
        }

        file_permissions_to_text(perm_get(attr.mode, S_ICUSR), &self->buf[1]);
        file_permissions_to_text(perm_get(attr.mode, S_ICGRP), &self->buf[4]);
        file_permissions_to_text(perm_get(attr.mode, S_ICOTH), &self->buf[7]);
        self->buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

        printf("%s %*d  %*u %*u  %*lld %*" PINID " %s\n",
            self->buf,
            self->linkCountWidth, attr.nlink,
            self->uidWidth, attr.uid,
            self->gidWidth, attr.gid,
            self->sizeWidth, attr.size,
            self->inodeIdWidth, attr.ino,
            entryName);
    }
    return err;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, fileName);
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

static errno_t iterate_dir(list_ctx_t* _Nonnull self, IOChannelRef _Nonnull chan, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    decl_try_err();
    ssize_t nBytesRead;

    while (err == EOK) {
        err = IOChannel_Read(chan, self->dirbuf, sizeof(self->dirbuf), &nBytesRead);
        if (err != EOK || nBytesRead == 0) {
            break;
        }

        const dir_entry_t* dep = self->dirbuf;
        
        while (nBytesRead > 0) {
            if (self->flags.printAll || dep->name[0] != '.') {
                err = cb(self, path, dep->name);
                if (err != EOK) {
                    break;
                }
            }

            nBytesRead -= sizeof(dir_entry_t);
            dep++;
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
    try(IOChannel_Seek(chan, 0ll, NULL, SEEK_SET));
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
    fs_attr_t attr;

    return (FileManager_GetAttributes(self->fm, path, &attr) == EOK && S_ISDIR(attr.mode)) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static errno_t do_list(FileManagerRef _Nonnull fm, const char* _Nonnull path, bool isPrintAll)
{
    decl_try_err();

    list_ctx_t* ctx = calloc(1, sizeof(list_ctx_t));
    if (ctx == NULL) {
        return ENOMEM;
    }

    ctx->fm = fm;
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
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    err = do_list(&m->fm, path, false);

catch:
    FSManager_Destroy(m);
    Object_Release(disk);
    return err;
}
