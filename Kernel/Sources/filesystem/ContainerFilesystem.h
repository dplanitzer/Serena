//
//  ContainerFilesystem.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ContainerFilesystem_h
#define ContainerFilesystem_h

#include <filesystem/Filesystem.h>
#include <filesystem/FSContainer.h>


// A filesystem which is stored inside of a FSContainer.
open_class(ContainerFilesystem, Filesystem,
    FSContainerRef _Nonnull fsContainer;
);
open_class_funcs(ContainerFilesystem, Filesystem,
);


//
// Methods for use by filesystem users.
//

// Creates an instance of a filesystem subclass. Users of a concrete filesystem
// should not use this function to allocate an instance of the concrete filesystem.
// This function is for use by Filesystem subclassers to define the filesystem
// specific instance allocation function.
extern errno_t ContainerFilesystem_Create(Class* pClass, FSContainerRef _Nonnull pContainer, FilesystemRef _Nullable * _Nonnull pOutFileSys);

// Returns the FSContainer.
#define Filesystem_GetContainer(__fs) \
    ((struct ContainerFilesystem*)(__fs))->fsContainer

#endif /* ContainerFilesystem_h */
