//
//  KfsSpecial.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsSpecial_h
#define KfsSpecial_h

#include "KfsNode.h"

    
open_class(KfsSpecial, KfsNode,
    ObjectRef _Nonnull  instance;
    intptr_t            arg;
);
open_class_funcs(KfsSpecial, KfsNode,
);


extern errno_t KfsSpecial_Create(KernFSRef _Nonnull kfs, ino_t inid, mode_t mode, uid_t uid, gid_t gid, ino_t pnid, ObjectRef _Nonnull fs, intptr_t arg, KfsNodeRef _Nullable * _Nonnull pOutSelf);

#endif /* KfsSpecial_h */
