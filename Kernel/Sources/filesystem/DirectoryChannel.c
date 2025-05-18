//
//  DirectoryChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DirectoryChannel.h"
#include "Filesystem.h"
#include <kpi/fcntl.h>


// DirectoryChannel uses the Inode lock to protect its seek state


// Creates a directory channel.
errno_t DirectoryChannel_Create(InodeRef _Nonnull pDir, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef self;

    try(IOChannel_Create(&kDirectoryChannelClass, kIOChannel_Seekable, SEO_FT_DIRECTORY, O_RDONLY, (IOChannelRef*)&self));
    self->inode = Inode_Reacquire(pDir);

catch:
    *pOutDir = (IOChannelRef)self;
    return err;
}

errno_t DirectoryChannel_finalize(DirectoryChannelRef _Nonnull self)
{
    decl_try_err();

    err = Inode_Relinquish(self->inode);
    self->inode = NULL;

    return err;
}

void DirectoryChannel_lock(DirectoryChannelRef _Nonnull self)
{
    Inode_Lock(self->inode);
}

void DirectoryChannel_unlock(DirectoryChannelRef _Nonnull _Locked self)
{
    Inode_Unlock(self->inode);
}

errno_t DirectoryChannel_read(DirectoryChannelRef _Nonnull _Locked self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Inode_Read(self->inode, self, pBuffer, nBytesToRead, nOutBytesRead);
}

errno_t DirectoryChannel_seek(DirectoryChannelRef _Nonnull _Locked self, off_t offset, off_t* _Nullable pOutOldPosition, int whence)
{
    decl_try_err();

    if (whence == SEEK_SET) {
        err = super_n(seek, IOChannel, DirectoryChannel, self, offset, pOutOldPosition, whence);
    }
    else {
        err = EINVAL;
    }

    return err;
}

void DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, struct stat* _Nonnull pOutInfo)
{
    IOChannel_Lock(self);
    Inode_GetInfo(self->inode, pOutInfo);
    IOChannel_Unlock(self);
}


class_func_defs(DirectoryChannel, IOChannel,
override_func_def(finalize, DirectoryChannel, IOChannel)
override_func_def(lock, DirectoryChannel, IOChannel)
override_func_def(unlock, DirectoryChannel, IOChannel)
override_func_def(read, DirectoryChannel, IOChannel)
override_func_def(seek, DirectoryChannel, IOChannel)
);
