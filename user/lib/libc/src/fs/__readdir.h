//
//  __readdir.h
//  libc
//
//  Created by Dietmar Planitzer on 4/5/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef __READDIR_H
#define __READDIR_H 1

#include <serena/directory.h>

__CPP_BEGIN

// 2kb
#define __DIRENT_COUNT 8

struct _DIR {
    // Read next set of entries if:
    // nextEntryToRead >= endOfBuffer
    dir_entry_t* _Nonnull nextEntryToRead;
    dir_entry_t* _Nonnull endOfBuffer;
    int                   fd;
    
    dir_entry_t           entbuf[__DIRENT_COUNT];
};

// Returns the underlying directory fd
#define _dir_fd(__self) \
(__self)->fd

__CPP_END

#endif /* __READDIR_H */
