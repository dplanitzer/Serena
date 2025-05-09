//
//  DirectoryChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DirectoryChannel_h
#define DirectoryChannel_h

#include <filesystem/IOChannel.h>
#include <filesystem/Inode.h>
#include <System/User.h>


// File positions/seeking and directories:
// The only allowed seeks are of the form seek(SEEK_SET) with an absolute position
// that was previously obtained from another seek or a value of 0 to rewind to the
// beginning of the directory listing. The meaning of the offset is filesystem
// specific. Ie it may represent a byte offset into the directory file or it may
// represent a directory entry number.
//
// Locking:
// See FileChannel.
open_class(DirectoryChannel, IOChannel,
    InodeRef _Nonnull   inode;
);
open_class_funcs(DirectoryChannel, IOChannel,
);


// Creates a directory object.
extern errno_t DirectoryChannel_Create(InodeRef _Nonnull pDir, IOChannelRef _Nullable * _Nonnull pOutDir);

#define DirectoryChannel_GetInode(__self) \
((DirectoryChannelRef)(__self))->inode

extern errno_t DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, finfo_t* _Nonnull pOutInfo);
extern errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, uid_t uid, gid_t gid, fmutinfo_t* _Nonnull pInfo);

#endif /* DirectoryChannel_h */
