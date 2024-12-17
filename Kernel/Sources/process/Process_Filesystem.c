//
//  Process_Filesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filesystem/FilesystemManager.h>
#include <System/Filesystem.h>


// Mounts the filesystem stored in the container at 'containerPath' at the
// directory 'atDirPath'. 'params' are optional mount parameters that are passed
// to the filesystem to mount.
errno_t Process_Mount(ProcessRef _Nonnull self, const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Mount(&self->fm, containerPath, atDirPath, params, paramsSize);
    Lock_Unlock(&self->lock);

    return err;
}

// Unmounts the filesystem mounted at the directory 'atDirPath'.
errno_t Process_Unmount(ProcessRef _Nonnull self, const char* _Nonnull atDirPath, uint32_t options)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    err = FileManager_Unmount(&self->fm, atDirPath, options);
    Lock_Unlock(&self->lock);

    return err;
}
