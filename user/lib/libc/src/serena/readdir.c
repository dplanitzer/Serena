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
#include <serena/directory.h>
#include <serena/fd.h>

// 2kb
#define DIRENT_COUNT 8

struct _DIR {
    // Read next set of entries if:
    // nextEntryToRead >= endOfBuffer
    dir_entry_t* _Nonnull nextEntryToRead;
    dir_entry_t* _Nonnull endOfBuffer;
    int                     fd;
    
    dir_entry_t           entbuf[DIRENT_COUNT];
};


dir_t* _Nullable dir_open(const char* _Nonnull path)
{
    dir_t* dir = malloc(sizeof(dir_t));

    if (dir) {
        if ((errno_t)_syscall(SC_dir_open, path, &dir->fd) != EOK) {
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
    if (dir) {
        const errno_t err = (errno_t)_syscall(SC_close, dir->fd);

        free(dir);

        return (err == EOK) ? 0 : -1;
    }
    else {
        return 0;
    }
}

void dir_rewind(dir_t* _Nonnull dir)
{
    (void)_syscall(SC_lseek, dir->fd, (off_t)0ll, NULL, SEEK_SET);
    dir->nextEntryToRead = dir->entbuf;
    dir->endOfBuffer = dir->entbuf;
}

const dir_entry_t* _Nullable dir_read(dir_t* _Nonnull dir)
{
    if (dir->nextEntryToRead >= dir->endOfBuffer) {
        ssize_t nBytesRead;
        const errno_t err = (errno_t)_syscall(SC_read, dir->fd, dir->entbuf, sizeof(dir_entry_t) * DIRENT_COUNT, &nBytesRead);

        if (err != EOK || nBytesRead == 0) {
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = (dir_entry_t*) (((char*)dir->entbuf) + nBytesRead);
    }

    dir_entry_t* dp = dir->nextEntryToRead;
    dir->nextEntryToRead++;

    return dp;
}
