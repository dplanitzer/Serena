//
//  ContainerFilesystem.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ContainerFilesystem.h"


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


class_func_defs(ContainerFilesystem, Filesystem,
override_func_def(deinit, ContainerFilesystem, Object)
);
