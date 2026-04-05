//
//  fs_label.c
//  libc
//
//  Created by Dietmar Planitzer on 4/4/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/fs.h>
#include <kpi/syscall.h>


int fs_label(fsid_t fsid, char* _Nonnull buf, size_t bufSize)
{
    return (int)_syscall(SC_fs_label, fsid, buf, bufSize);
}

int fs_setlabel(fsid_t fsid, const char* _Nonnull label)
{
    return (int)_syscall(SC_fs_setlabel, fsid, label);
}
