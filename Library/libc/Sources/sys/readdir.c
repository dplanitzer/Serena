//
//  opendir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <kpi/syscall.h>

// 2kb
#define DIRENT_COUNT 8

struct _DIR {
    // Read next set of entries if:
    // nextEntryToRead >= endOfBuffer
    struct dirent* _Nonnull nextEntryToRead;
    struct dirent* _Nonnull endOfBuffer;
    int                     fd;
    
    struct dirent           entbuf[DIRENT_COUNT];
};


DIR* _Nullable opendir(const char* _Nonnull path)
{
    DIR* dir = malloc(sizeof(DIR));

    if (dir) {
        if ((errno_t)_syscall(SC_opendir, path, &dir->fd) != EOK) {
            free(dir);
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = dir->entbuf;
    }
    return dir;
}

int closedir(DIR* _Nullable dir)
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

void rewinddir(DIR* _Nonnull dir)
{
    (void)_syscall(SC_lseek, dir->fd, (off_t)0ll, NULL, SEEK_SET);
    dir->nextEntryToRead = dir->entbuf;
    dir->endOfBuffer = dir->entbuf;
}

struct dirent* _Nullable readdir(DIR* _Nonnull dir)
{
    if (dir->nextEntryToRead >= dir->endOfBuffer) {
        ssize_t nBytesRead;
        const errno_t err = (errno_t)_syscall(SC_read, dir->fd, dir->entbuf, sizeof(struct dirent) * DIRENT_COUNT, &nBytesRead);

        if (err != EOK || nBytesRead == 0) {
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = (struct dirent*) (((char*)dir->entbuf) + nBytesRead);
    }

    struct dirent* dp = dir->nextEntryToRead;
    dir->nextEntryToRead++;

    return dp;
}
