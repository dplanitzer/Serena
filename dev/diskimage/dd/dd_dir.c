//
//  dd_dir.c
//  libc
//
//  Created by Dietmar Planitzer on 11/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "dd_dir.h"
#include <stddef.h>
#include <stdlib.h>
#include <kpi/directory.h>
#include <kpi/types.h>
#include <handler/Handler.h>


// 2kb
#define __DIRENT_COUNT 8

struct dd_dir {
    HandlerRef      hnd;

    // Read next set of entries if:
    // nextEntryToRead >= endOfBuffer
    dir_entry_t*    nextEntryToRead;
    dir_entry_t*    endOfBuffer;
    
    dir_entry_t  entbuf[__DIRENT_COUNT];
};


dd_dir_t dd_opendir(FSManagerRef fsm, const char* path)
{
    dd_dir_t dir = malloc(sizeof(struct dd_dir));

    if (dir) {
        const errno_t err = FileManager_OpenDirectory(&fsm->fm, path, &dir->hnd);

        if (err != EOK) {
            free(dir);
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = dir->entbuf;
    }
    return dir;
}

int dd_closedir(dd_dir_t dir)
{
    int r = 0;

    if (dir) {
        Object_Release(dir->hnd);
        dir->hnd = NULL;
        free(dir);
    }
    return r;
}

void dd_rewinddir(dd_dir_t dir)
{
    Handler_Seek(dir->hnd, (off_t)0ll, NULL, SEEK_SET);
    dir->nextEntryToRead = dir->entbuf;
    dir->endOfBuffer = dir->entbuf;
}

const dd_dir_entry_t* dd_dirnext(dd_dir_t dir)
{
    if (dir->nextEntryToRead >= dir->endOfBuffer) {
        ssize_t nBytesRead;
        const errno_t err = Handler_Read(dir->hnd, dir->entbuf, sizeof(dir_entry_t) * __DIRENT_COUNT, &nBytesRead);

        if (err != EOK || nBytesRead == 0) {
            return NULL;
        }

        dir->nextEntryToRead = dir->entbuf;
        dir->endOfBuffer = (dir_entry_t*) (((char*)dir->entbuf) + nBytesRead);
    }

    dir_entry_t* dp = dir->nextEntryToRead;
    dir->nextEntryToRead++;

    return (dd_dir_entry_t*)dp;
}
