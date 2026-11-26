//
//  dd.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "dd.h"
#include "FSManager.h"
#include <handler/Handler.h>
#include <errno.h>


int dd_path_kind(FSManagerRef fsm, const char *path)
{
    fs_attr_t attr;
    errno_t err = FileManager_GetAttributes(&fsm->fm, path, &attr);

    if (err != EOK) {
        errno = err;
        return DD_KIND_ERROR;
    }

    return (attr.file_type == FS_FTYPE_DIR) ? DD_KIND_DIRECTORY : DD_KIND_OTHER;
}

int dd_makedir(FSManagerRef fsm, const char *path, fs_perms_t perms)
{
    const errno_t err = FileManager_CreateDirectory(&fsm->fm, path, perms);
    
    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_remove(FSManagerRef fsm, const char *path)
{
    const errno_t err = FileManager_Unlink(&fsm->fm, path, __FS_ULNK_ANY);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_chown(FSManagerRef fsm, const char *path, uid_t uid, gid_t gid)
{
    const errno_t err = FileManager_SetFileOwner(&fsm->fm, path, uid, gid);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_chperm(FSManagerRef fsm, const char *path, fs_perms_t perms)
{
    const errno_t err = FileManager_SetFilePermissions(&fsm->fm, path, perms);

    if (err == EOK) {
        return 0;
    }
    else {
        errno = err;
        return -1;
    }
}

int dd_attr(FSManagerRef fsm, const char *path, fs_attr_t *attr)
{
    const errno_t err = FileManager_GetAttributes(&fsm->fm, path, attr);

    if (err == EOK) {
        return 0;
    } 
    else {
        errno = err;
        return -1;
    }
}


dd_fd_t dd_open(FSManagerRef fsm, const char *path, int flags)
{
    HandlerRef hnd;
    fd_flags_t oflags = O_RDONLY;

    if ((flags & DD_WRONLY) == DD_WRONLY) {
        oflags |= O_WRONLY;
    }
    if ((flags & DD_TRUNC) == DD_TRUNC) {
        oflags |= O_TRUNC;
    }

    const errno_t err = FileManager_OpenFile(&fsm->fm, path, oflags, &hnd);
    if (err == EOK) {
        return hnd;
    }
    else {
        errno = err;
        return NULL;
    }
}

dd_fd_t dd_create(FSManagerRef fsm, const char *path, int flags, fs_perms_t perms)
{
    HandlerRef hnd;
    fd_flags_t oflags = O_RDONLY;

    if ((flags & DD_WRONLY) == DD_WRONLY) {
        oflags |= O_WRONLY;
    }

    const errno_t err = FileManager_CreateFile(&fsm->fm, path, oflags, perms, &hnd);
    if (err == EOK) {
        return hnd;
    }
    else {
        errno = err;
        return NULL;
    }
}

int dd_close(dd_fd_t fd)
{
    if (fd) {
        Object_Release((HandlerRef)fd);
    }

    return 0;
}

ssize_t dd_read(dd_fd_t fd, char *buf, size_t count)
{
    ssize_t nBytesRead = 0;
    const errno_t err = Handler_Read((HandlerRef)fd, buf, count, &nBytesRead);

    if (err == EOK) {
        return nBytesRead;
    }
    else {
        errno = err;
        return -1;
    }
}

ssize_t dd_write(dd_fd_t fd, const char *buf, size_t count)
{
    ssize_t nBytesWritten = 0;
    const errno_t err = Handler_Write((HandlerRef)fd, buf, count, &nBytesWritten);

    if (err == EOK) {
        return nBytesWritten;
    }
    else {
        errno = err;
        return -1;
    }
}
