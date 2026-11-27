//
//  cmd_list.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ext/stdlib.h>
#include <dd/dd.h>
#include <kpi/fs_perms.h>

#define PINID   PRIu32
//#define PINID   PRIu64


// Buffer holding directory entries
#define ENTBUF_COUNT 16

// Buffer used for various conversions
#define BUF_SIZE    32

// Max length of a permission string
#define PERMISSIONS_STRING_LENGTH  11


typedef struct list_ctx {
    dd_fs_t _Nonnull            fs;

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
    char                        pathbuf[DD_PATH_MAX];
} list_ctx_t;


typedef int (*dir_iter_t)(list_ctx_t* _Nonnull self, const char* _Nonnull dirPath, const char* _Nonnull entryName);


static void file_permissions_to_text(fs_perms_t perms, char* _Nonnull buf)
{
    if ((perms & FS_PRM_R) == FS_PRM_R) {
        buf[0] = 'r';
    }
    if ((perms & FS_PRM_W) == FS_PRM_W) {
        buf[1] = 'w';
    }
    if ((perms & FS_PRM_X) == FS_PRM_X) {
        buf[2] = 'x';
    }
}

static int format_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;

    if (dd_attr(self->fs, path, &attr) < 0) {
        return -1;
    }
    
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

    return 0;
}

static int print_inode(list_ctx_t* _Nonnull self, const char* _Nonnull path, const char* _Nonnull entryName)
{
    fs_attr_t attr;
    char tc;

    if (dd_attr(self->fs, path, &attr) < 0) {
        return -1;
    }


    switch (attr.file_type) {
        case FS_FTYPE_DEV:   tc = 'h'; break;
        case FS_FTYPE_DIR:   tc = 'd'; break;
        case FS_FTYPE_FIFO:   tc = 'p'; break;
        case FS_FTYPE_LNK:   tc = 'l'; break;
        default:        tc = '-'; break;
    }
    self->buf[0] = tc;

    for (int i = 1; i < PERMISSIONS_STRING_LENGTH; i++) {
        self->buf[i] = '-';
    }

    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_USR), &self->buf[1]);
    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_GRP), &self->buf[4]);
    file_permissions_to_text(fs_perms_get(attr.permissions, FS_CLS_OTH), &self->buf[7]);
    self->buf[PERMISSIONS_STRING_LENGTH - 1] = '\0';

    printf("%s %*d  %*u %*u  %*lld %*" PINID " %s\n",
        self->buf,
        self->linkCountWidth, attr.nlink,
        self->uidWidth, attr.uid,
        self->gidWidth, attr.gid,
        self->sizeWidth, attr.size,
        self->inodeIdWidth, attr.ino,
        entryName
    );

    return 0;
}


static void concat_path(char* _Nonnull path, const char* _Nonnull dir, const char* _Nonnull fileName)
{
    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, fileName);
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

static int iterate_dir(list_ctx_t* _Nonnull self, dd_dir_t _Nonnull dir, const char* _Nonnull path, dir_iter_t _Nonnull cb)
{
    errno = 0;

    for (;;) {
        const dir_entry_t* dep = dd_dirnext(dir);
        
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

static int list_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    dd_dir_t dir = dd_opendir(self->fs, path);
    int r = -1;

    if (dir) {
        r = iterate_dir(self, dir, path, format_dir_entry);

        if (!r) {
            dd_rewinddir(dir);
            iterate_dir(self, dir, path, print_dir_entry);
        }
    
        dd_closedir(dir);
    }

    return r;
}

static errno_t list_file(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    if (!format_inode(self, path, path)) {
        print_inode(self, path, path);
        return 0;
    }

    return -1;
}

static bool is_dir(list_ctx_t* _Nonnull self, const char* _Nonnull path)
{
    return (dd_path_kind(self->fs, path) == DD_KIND_DIRECTORY) ? true : false;
}


////////////////////////////////////////////////////////////////////////////////

static list_ctx_t g_list_ctx;

errno_t cmd_list(const char* _Nonnull dmgPath, const clap_string_array_t* _Nonnull paths)
{
    if (paths->count < 1) {
        return ERANGE;
    }

    dd_fs_t fs = dd_mount(dmgPath);
    if (fs == NULL) {
        return errno;
    }

    for (size_t i = 0; i < paths->count; i++) {
        const char* cur_path = paths->strings[i];
        int r;

        if (dd_validate_path(cur_path) < 0) {
            break;
        }

        memset(&g_list_ctx, 0, sizeof(list_ctx_t));
        g_list_ctx.fs = fs;
        g_list_ctx.flags.printAll = false;

        if (is_dir(&g_list_ctx, cur_path)) {
            r = list_dir(&g_list_ctx, cur_path);
        }
        else {
            r = list_file(&g_list_ctx, cur_path);
        }

        if (r < 0) {
            break;
        }
    }

    dd_unmount(fs, 0);
    return errno;
}
