//
//  perm_check.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef PERM_CHECK_H_
#define PERM_CHECK_H_

#include <ext/try.h>
#ifndef __DISKIMAGE__
#include <kern/signal.h>
#endif
#include <kobj/AnyRefs.h>
#include <kpi/types.h>

extern errno_t perm_check_node_access(InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode);
extern errno_t perm_check_node_attr_update(InodeRef _Nonnull _Locked pNode, uid_t uid);

#ifndef __DISKIMAGE__
extern errno_t perm_check_send_signal(const sig_sndr_t* _Nonnull sndr, int rcv_scope, pid_t rcv_pid, uid_t rcv_uid, int signo);
#endif

#endif /* PERM_CHECK_H_ */
