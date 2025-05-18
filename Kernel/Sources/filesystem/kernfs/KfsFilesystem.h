//
//  KfsFilesystem.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsFilesystem_h
#define KfsFilesystem_h

#include "KfsNode.h"
#include <filesystem/Filesystem.h>

    
open_class(KfsFilesystem, KfsNode,
    FilesystemRef _Nonnull  instance;
);
open_class_funcs(KfsFilesystem, KfsNode,
);


extern errno_t KfsFilesystem_Create(KernFSRef _Nonnull kfs, ino_t inid, mode_t permissions, uid_t uid, gid_t gid, ino_t pnid, FilesystemRef _Nonnull fs, KfsNodeRef _Nullable * _Nonnull pOutSelf);

#endif /* KfsFilesystem_h */
