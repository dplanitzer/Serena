//
//  RamFS.h
//  Apollo
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef RamFS_h
#define RamFS_h

#include "Filesystem.h"

OPAQUE_CLASS(RamFS, Filesystem);
typedef struct _RamFSMethodTable {
    FilesystemMethodTable   super;
} RamFSMethodTable;


// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
ErrorCode RamFS_Create(RamFSRef _Nullable * _Nonnull pOutFileSys);

#endif /* RamFS_h */
