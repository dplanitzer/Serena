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
#include <User.h>
#include <dispatcher/Lock.h>


// File positions/seeking and directories:
// The only allowed seeks are of the form seek(kSeek_Set) with an absolute position
// that was previously obtained from another seek or a value of 0 to rewind to the
// beginning of the directory listing. The meaning of the offset is filesystem
// specific. Ie it may represent a byte offset into the directory file or it may
// represent a directory entry number.
//
// Locking:
// See FileChannel.
open_class_with_ref(DirectoryChannel, IOChannel,
    FileOffset          offset;
    InodeRef _Nonnull   inode;
    Lock                lock;
);
open_class_funcs(DirectoryChannel, IOChannel,
);


// Creates a directory object.
extern errno_t DirectoryChannel_Create(InodeRef _Consuming _Nonnull pNode, IOChannelRef _Nullable * _Nonnull pOutDir);

extern errno_t DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);

#endif /* DirectoryChannel_h */
