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


// Creates an instance of RamFS. RamFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
ErrorCode RamFS_Create(User rootDirUser, RamFSRef _Nullable * _Nonnull pOutFileSys);

#endif /* RamFS_h */
