//
//  SerenaFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFS_h
#define SerenaFS_h

#include <filesystem/ContainerFilesystem.h>


final_class(SerenaFS, ContainerFilesystem);


// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
extern errno_t SerenaFS_FormatDrive(FSContainerRef _Nonnull pContainer, uid_t uid, gid_t gid, FilePermissions permissions);


// Creates an instance of SerenaFS.
extern errno_t SerenaFS_Create(FSContainerRef _Nonnull pContainer, SerenaFSRef _Nullable * _Nonnull pOutSelf);

#endif /* SerenaFS_h */
