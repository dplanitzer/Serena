//
//  KfsProcess.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/10/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsProcess_h
#define KfsProcess_h

#include "KfsNode.h"

    
open_class(KfsProcess, KfsNode,
    ProcessRef _Nonnull  instance;
);
open_class_funcs(KfsProcess, KfsNode,
);


extern errno_t KfsProcess_Create(KernFSRef _Nonnull kfs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, ProcessRef _Nonnull proc, KfsNodeRef _Nullable * _Nonnull pOutSelf);

#endif /* KfsProcess_h */
