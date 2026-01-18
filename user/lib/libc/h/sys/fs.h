//
//  sys/fs.h
//  libc
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FS_H
#define _SYS_FS_H 1

#include <_cmndef.h>
#include <kpi/fs.h>

// Returns the path to the disk driver that underpins the filesystem with the
// given id.
extern int fs_getdisk(fsid_t fsid, char* _Nonnull buf, size_t bufSize);

#endif /* _SYS_FS_H */
