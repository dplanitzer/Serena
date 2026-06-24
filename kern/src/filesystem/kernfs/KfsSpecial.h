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
#include "KernFS.h"


open_class(KfsSpecial, KfsNode,
    ObjectRef _Nullable             resource;
    KfsCreateHandlerFunc _Nonnull   createHandlerFunc;
);
open_class_funcs(KfsSpecial, KfsNode,
);


extern errno_t KfsSpecial_Create(KernFSRef _Nonnull kfs, ino_t inid, ino_t pnid, const KfsHandlerNodeArgs* args, KfsNodeRef _Nullable * _Nonnull pOutSelf);

#define KfsSpecial_GetResource(__self) \
((KfsSpecialRef)(__self))->resource

#endif /* KfsSpecial_h */
