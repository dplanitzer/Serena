//
//  ContainerFilesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ContainerFilesystem.h"
#include <kpi/file.h>


errno_t ContainerFilesystem_Create(Class* pClass, FSContainerRef _Nonnull pContainer, FilesystemRef _Nullable * _Nonnull pOutFileSys)
{
    decl_try_err();
    struct ContainerFilesystem* self = NULL;

    if ((err = Filesystem_Create(pClass, (FilesystemRef*)&self)) == EOK) {
        self->fsContainer = Object_RetainAs(pContainer, FSContainer);
    }

    *pOutFileSys = (FilesystemRef)self;
    return err;
}

void ContainerFilesystem_deinit(struct ContainerFilesystem* _Nonnull self)
{
    Object_Release(self->fsContainer);
    self->fsContainer = NULL;
}

void ContainerFilesystem_onDisconnect(struct ContainerFilesystem* _Nonnull self)
{
    FSContainer_Disconnect(self->fsContainer);
}

size_t ContainerFilesystem_getNodeBlockSize(struct ContainerFilesystem* _Nonnull self, InodeRef _Locked _Nonnull node)
{
    return FSContainer_GetBlockSize(self->fsContainer);
}

errno_t ContainerFilesystem_getInfo(struct ContainerFilesystem* _Nonnull self, int flavor, fs_info_ref _Nonnull pOutInfo)
{
    decl_try_err();

    switch (flavor) {
        case FS_INFO_DISK: {
            disk_info_t* ip = pOutInfo;

            err = FSContainer_GetDiskInfo(Filesystem_GetContainer(self), ip);
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    return err;
}

void ContainerFilesystem_sync(struct ContainerFilesystem* _Nonnull self)
{
    super_0(sync, Filesystem, ContainerFilesystem, self);
    FSContainer_Sync(self->fsContainer);
}

errno_t ContainerFilesystem_ioctl(struct ContainerFilesystem* _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kFSCommand_GetDiskInfo: {
            disk_info_t* info = va_arg(ap, disk_info_t*);

            return FSContainer_GetDiskInfo(Filesystem_GetContainer(self), info);
        }

        default:
            return super_n(ioctl, Filesystem, ContainerFilesystem, self, pChannel, cmd, ap);
    }
}


class_func_defs(ContainerFilesystem, Filesystem,
override_func_def(deinit, ContainerFilesystem, Object)
override_func_def(onDisconnect, ContainerFilesystem, Filesystem)
override_func_def(getNodeBlockSize, ContainerFilesystem, Filesystem)
override_func_def(getInfo, ContainerFilesystem, Filesystem)
override_func_def(sync, ContainerFilesystem, Filesystem)
override_func_def(ioctl, ContainerFilesystem, Filesystem)
);
