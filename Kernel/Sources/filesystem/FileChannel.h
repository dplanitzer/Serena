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


OPEN_CLASS_WITH_REF(FileChannel, IOChannel,
    ObjectRef _Nonnull  filesystem;
    InodeRef _Nonnull   inode;
    FileOffset          offset;
);

typedef struct _FileChannelMethodTable {
    IOChannelMethodTable    super;
} FileChannelMethodTable;


// Creates a file object.
extern errno_t FileChannel_Create(ObjectRef _Nonnull pFilesystem, unsigned int mode, InodeRef _Nonnull pNode, IOChannelRef _Nullable * _Nonnull pOutFile);

extern errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);

extern errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length);

#endif /* FileChannel_h */
