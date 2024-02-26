//
//  SerenaFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFS_h
#define SerenaFS_h

#include "Filesystem.h"

OPAQUE_CLASS(SerenaFS, Filesystem);
typedef struct _SerenaFSMethodTable {
    FilesystemMethodTable   super;
} SerenaFSMethodTable;


// Creates an instance of SerenaFS. SerenaFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
errno_t SerenaFS_Create(User rootDirUser, SerenaFSRef _Nullable * _Nonnull pOutFileSys);

#endif /* SerenaFS_h */
