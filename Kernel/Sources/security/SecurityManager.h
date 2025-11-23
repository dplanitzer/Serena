//
//  SecurityManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SecurityManager_h
#define SecurityManager_h

#include <kobj/Object.h>
#include <filesystem/Inode.h>

typedef struct sigcred {
    pid_t   pid;
    pid_t   ppid;
    uid_t   uid;
} sigcred_t;


extern SecurityManagerRef _Nonnull  gSecurityManager;

extern errno_t SecurityManager_Create(SecurityManagerRef _Nullable * _Nonnull pOutSelf);

extern errno_t SecurityManager_CheckNodeAccess(SecurityManagerRef _Nonnull self, InodeRef _Nonnull _Locked pNode, uid_t uid, gid_t gid, int mode);
extern errno_t SecurityManager_CheckNodeStatusUpdatePermission(SecurityManagerRef _Nonnull self, InodeRef _Nonnull _Locked pNode, uid_t uid);

extern bool SecurityManager_CanSendSignal(SecurityManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, const sigcred_t* _Nonnull rcv, int signo);

extern bool SecurityManager_IsSuperuser(SecurityManagerRef _Nonnull self, uid_t uid);

#endif /* SecurityManager_h */
