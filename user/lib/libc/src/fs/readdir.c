//
//  readdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <stddef.h>
#include <stdlib.h>
#include <ext/errno.h>
#include <kpi/syscall.h>
#include <serena/fd.h>
#include "__readdir.h"

dir_t* _Nullable dir_open(dir_t* _Nullable wd, const char* _Nonnull path)
{
    dir_t* dir = malloc(sizeof(dir_t));

    if (dir) {
        if (_syscall(SC_dir_open, (wd) ? _dir_fd(wd) : FD_CWD, path, &dir->fd) != 0) {
            free(dir);
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = dir->entbuf;
    }
    return dir;
}

int dir_close(dir_t* _Nullable dir)
{
    int r = 0;

    if (dir) {
        r = _syscall(SC_fd_close, dir->fd);
        free(dir);
    }
    return r;
}

void dir_rewind(dir_t* _Nonnull dir)
{
    (void)_syscall(SC_fd_seek, dir->fd, (off_t)0ll, NULL, SEEK_SET);
    dir->nextEntryToRead = dir->entbuf;
    dir->endOfBuffer = dir->entbuf;
}

const dir_entry_t* _Nullable dir_read(dir_t* _Nonnull dir)
{
    if (dir->nextEntryToRead >= dir->endOfBuffer) {
        ssize_t nBytesRead;
        const int r = _syscall(SC_fd_read, dir->fd, dir->entbuf, sizeof(dir_entry_t) * __DIRENT_COUNT, &nBytesRead);

        if (r != 0 || nBytesRead == 0) {
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = (dir_entry_t*) (((char*)dir->entbuf) + nBytesRead);
    }

    dir_entry_t* dp = dir->nextEntryToRead;
    dir->nextEntryToRead++;

    return dp;
}
