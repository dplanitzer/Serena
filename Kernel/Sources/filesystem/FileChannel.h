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


// The purpose of the lock:
// 1) It ensures that if two or more concurrent reads/writes are issued for the
//    exact same byte range in the file, that once all reads/writes have finished
//    that all bytes are defined by the result of a single read/write operation.
//    Expressed differently, two writes to the same byte range will never result
//    in writing a mix of bytes from both writes. It's either all bytes from the
//    first or all bytes from the second write.
// 2) File offset changes are atomic.
// 3) It is permissible and safe to issue a close() on one thread of execution
//    while reads/writes are in progress on other threads of execution. The read
//    and write operations will correctly detect whether the channel is still
//    open or closed and proceed accordingly. Note that this means currently that
//    a close() does not cancel an ongoing operation. It has to wait until the
//    currently executing operation finishes.
OPEN_CLASS_WITH_REF(FileChannel, IOChannel,
    Lock                lock;
    ObjectRef _Nonnull  filesystem;
    InodeRef _Nonnull   inode;
    FileOffset          offset;
);
typedef struct FileChannelMethodTable {
    IOChannelMethodTable    super;
} FileChannelMethodTable;


// Creates a file object.
extern errno_t FileChannel_Create(ObjectRef _Consuming _Nonnull pFilesystem, InodeRef _Consuming _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile);

extern errno_t FileChannel_GetInfo(FileChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo);
extern errno_t FileChannel_SetInfo(FileChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo);

extern errno_t FileChannel_Truncate(FileChannelRef _Nonnull self, User user, FileOffset length);

#endif /* FileChannel_h */
