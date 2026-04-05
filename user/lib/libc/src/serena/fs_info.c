//
//  fs_info.c
//  libc
//
//  Created by Dietmar Planitzer on 4/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fs.h>
#include <kpi/syscall.h>


int fs_info(fsid_t fsid, int flavor, fs_info_ref _Nonnull info)
{
    return (int)_syscall(SC_fs_info, fsid, flavor, info);
}
