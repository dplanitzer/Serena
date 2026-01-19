//
//  sefs_init.h
//  diskimage
//
//  Created by Dietmar Planitzer on 4/16/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef sefs_init_h
#define sefs_init_h

#include <stdint.h>
#include <ext/try.h>
#include <kpi/stat.h>
#include <kpi/types.h>


typedef errno_t (*sefs_block_write_t)(intptr_t fd, const void* _Nonnull buf, blkno_t blockAddr, size_t blockSize);


// Initializes the given disk drive with an instance of SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
extern errno_t sefs_init(intptr_t fd, sefs_block_write_t _Nonnull bwFunc, blkcnt_t blockCount, size_t blockSize, const struct timespec* creatTime, uid_t uid, gid_t gid, mode_t permissions, const char* _Nonnull label);

#endif /* sefs_init_h */
