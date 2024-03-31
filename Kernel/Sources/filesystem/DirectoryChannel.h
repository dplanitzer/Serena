//
//  DirectoryChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DirectoryChannel_h
#define DirectoryChannel_h

#include "IOChannel.h"
#include "Inode.h"


// File positions/seeking and directories:
// The only allowed seeks are of the form seek(SEEK_SET) with an absolute position
// that was previously obtained from another seek or a value of 0 to rewind to the
// beginning of the directory listing. The seek position represents the index of
// the first directory entry that should be returned by the next read() operation.
// It is not a byte offset. This way it doesn't matter to the user of the read()
// and seek() call how exactly the contents of a directory is stored in the file
// system.  
OPEN_CLASS_WITH_REF(DirectoryChannel, IOChannel,
    ObjectRef _Nonnull  filesystem;
    InodeRef _Nonnull   inode;
    FileOffset          offset;
);

typedef struct _DirectoryChannelMethodTable {
    IOChannelMethodTable    super;
} DirectoryChannelMethodTable;


// Creates a directory object.
extern errno_t DirectoryChannel_Create(ObjectRef _Nonnull pFilesystem, InodeRef _Nonnull pNode, IOChannelRef _Nullable * _Nonnull pOutDir);

extern errno_t DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);

#endif /* DirectoryChannel_h */
