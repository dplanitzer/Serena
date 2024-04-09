//
//  DirectoryChannel.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DirectoryChannel.h"
#include "Filesystem.h"


// Creates a directory channel which takes ownership of the provided filesystem
// and inode references. These references will be released by deinit().
errno_t DirectoryChannel_Create(ObjectRef _Consuming _Nonnull pFilesystem, InodeRef _Consuming _Nonnull pNode, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef self;

    try(IOChannel_AbstractCreate(&kDirectoryChannelClass, kOpen_Read, (IOChannelRef*)&self));
    self->filesystem = pFilesystem;
    self->inode = pNode;
    self->offset = 0ll;

catch:
    *pOutDir = (IOChannelRef)self;
    return err;
}

void DirectoryChannel_deinit(DirectoryChannelRef _Nonnull self)
{
    if (self->inode) {
        Filesystem_RelinquishNode((FilesystemRef)self->filesystem, self->inode);
        self->inode = NULL;
    }
    if (self->filesystem) {
        Object_Release(self->filesystem);
        self->filesystem = NULL;
    }
}

// Creates an independent copy of the receiver. The copy receives its own strong
// filesystem and inode references.
ssize_t DirectoryChannel_dup(DirectoryChannelRef _Nonnull self, IOChannelRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();
    DirectoryChannelRef pNewDir;

    try(IOChannel_AbstractCreateCopy((IOChannelRef)self, (IOChannelRef*)&pNewDir));
    pNewDir->filesystem = Object_Retain(self->filesystem);
    pNewDir->inode = Filesystem_ReacquireUnlockedNode((FilesystemRef)self->filesystem, self->inode);
    pNewDir->offset = self->offset;

catch:
    *pOutDir = (IOChannelRef)pNewDir;
    return err;
}

errno_t DirectoryChannel_ioctl(DirectoryChannelRef _Nonnull self, int cmd, va_list ap)
{
    switch (cmd) {
        case kIOChannelCommand_GetType:
            *((int*) va_arg(ap, int*)) = kIOChannelType_Directory;
            return EOK;

        default:
            return Object_SuperN(ioctl, IOChannel, self, cmd, ap);
    }
}

errno_t DirectoryChannel_read(DirectoryChannelRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    return Filesystem_ReadDirectory((FilesystemRef)self->filesystem, self->inode, pBuffer, nBytesToRead, &self->offset, nOutBytesRead);
}

errno_t DirectoryChannel_seek(DirectoryChannelRef _Nonnull self, FileOffset offset, FileOffset* _Nullable pOutOldPosition, int whence)
{
    
    if(pOutOldPosition) {
        *pOutOldPosition = self->offset;
    }
    if (whence != kSeek_Set || offset < 0) {
        return EINVAL;
    }
    if (offset > (FileOffset)INT_MAX) {
        return EOVERFLOW;
    }

    self->offset = offset;
    return EOK;
}

errno_t DirectoryChannel_GetInfo(DirectoryChannelRef _Nonnull self, FileInfo* _Nonnull pOutInfo)
{
    return Filesystem_GetFileInfo((FilesystemRef)self->filesystem, self->inode, pOutInfo);
}

errno_t DirectoryChannel_SetInfo(DirectoryChannelRef _Nonnull self, User user, MutableFileInfo* _Nonnull pInfo)
{
    return Filesystem_SetFileInfo((FilesystemRef)self->filesystem, self->inode, user, pInfo);
}


CLASS_METHODS(DirectoryChannel, IOChannel,
OVERRIDE_METHOD_IMPL(deinit, DirectoryChannel, Object)
OVERRIDE_METHOD_IMPL(dup, DirectoryChannel, IOChannel)
OVERRIDE_METHOD_IMPL(ioctl, DirectoryChannel, IOChannel)
OVERRIDE_METHOD_IMPL(read, DirectoryChannel, IOChannel)
OVERRIDE_METHOD_IMPL(seek, DirectoryChannel, IOChannel)
);
