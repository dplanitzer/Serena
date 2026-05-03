//
//  perm_check.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef PERM_CHECK_H_
#define PERM_CHECK_H_

#include <filesystem/Inode.h>

typedef struct sigcred {
    pid_t   pid;
    pid_t   ppid;
    uid_t   uid;
} sigcred_t;


extern errno_t perm_check_node_access(InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode);
extern errno_t perm_check_node_attr_update(InodeRef _Nonnull _Locked pNode, uid_t uid);

extern bool perm_check_send_signal(const sigcred_t* _Nonnull sndr, const sigcred_t* _Nonnull rcv, int signo);

extern bool perm_check_suser(uid_t uid);

#endif /* PERM_CHECK_H_ */
