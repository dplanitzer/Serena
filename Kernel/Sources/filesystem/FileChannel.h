//
//  FileChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FileChannel_h
#define FileChannel_h

#include <filesystem/IOChannel.h>
#include <filesystem/Inode.h>
#include <User.h>


open_class(FileChannel, IOChannel,
    InodeRef _Nonnull   inode;
);
open_class_funcs(FileChannel, IOChannel,
);


// Creates a file object.
extern errno_t FileChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile);

#define FileChannel_GetInode(__self) \
((FileChannelRef)(__self))->inode


extern off_t FileChannel_GetFileSize(FileChannelRef _Nonnull self);

extern errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, uid_t uid, gid_t gid, MutableFileInfo* _Nonnull pInfo);

extern errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, off_t length);

#endif /* FileChannel_h */
