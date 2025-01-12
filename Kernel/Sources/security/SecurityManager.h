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


extern SecurityManagerRef _Nonnull  gSecurityManager;

extern errno_t SecurityManager_Create(SecurityManagerRef _Nullable * _Nonnull pOutSelf);

extern errno_t SecurityManager_CheckNodeAccess(SecurityManagerRef _Nonnull self, InodeRef _Nonnull _Locked pNode, UserId uid, GroupId gid, AccessMode mode);

extern bool SecurityManager_IsSuperuser(SecurityManagerRef _Nonnull self, UserId uid);

#endif /* SecurityManager_h */
