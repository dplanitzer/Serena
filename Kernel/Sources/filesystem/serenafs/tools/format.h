//
//  format.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef sefs_format_h
#define sefs_format_h

#include <stdint.h>
#ifdef __KERNEL__
#include <kern/try.h>
#include <kern/types.h>
#include <kpi/stat.h>
#else
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif


typedef errno_t (*sefs_block_write_t)(intptr_t fd, const void* _Nonnull buf, blkno_t blockAddr, size_t blockSize);


// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
extern errno_t sefs_format(intptr_t fd, sefs_block_write_t _Nonnull bwFunc, blkcnt_t blockCount, size_t blockSize, uid_t uid, gid_t gid, mode_t permissions, const char* _Nonnull label);

#endif /* sefs_format_h */
