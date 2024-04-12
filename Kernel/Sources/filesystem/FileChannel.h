//
//  FileChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FileChannel_h
#define FileChannel_h

#include "IOChannel.h"
#include "Inode.h"
#include <dispatcher/Lock.h>


open_class_with_ref(FileChannel, IOChannel,
    ObjectRef _Nonnull  filesystem;
    InodeRef _Nonnull   inode;
    Lock                lock;           // Protects 'offset' and to provide read/write/seek serialization. See IOChannel
    FileOffset          offset;
);
open_class_funcs(FileChannel, IOChannel,
);


// Creates a file object.
extern errno_t FileChannel_Create(ObjectRef _Consuming _Nonnull pFilesystem, InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile);

extern errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);

extern errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length);

#endif /* FileChannel_h */
