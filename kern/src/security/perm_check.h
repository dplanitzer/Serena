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
#include <kpi/types.h>
#include <kobj/AnyRefs.h>

typedef struct sigcred {
    pid_t   pid;
    pid_t   ppid;
    uid_t   uid;
} sigcred_t;


extern errno_t perm_check_node_access(InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode);
extern errno_t perm_check_node_attr_update(InodeRef _Nonnull _Locked pNode, uid_t uid);

extern errno_t perm_check_send_signal(const sigcred_t* _Nonnull sndr, const sigcred_t* _Nonnull rcv, int signo);

#endif /* PERM_CHECK_H_ */
