//
//  proc_self.c
//  libc
//
//  Created by Dietmar Planitzer on 3/29/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


pid_t proc_self(void)
{
    proc_ids_info_t info;

    (void)_syscall(SC_proc_info, PROC_SELF, PROC_INFO_IDS, &info);
    return info.id;
}
