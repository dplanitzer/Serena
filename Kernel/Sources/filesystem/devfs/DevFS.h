//
//  DevFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/3/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DevFS_h
#define DevFS_h

#include <filesystem/Filesystem.h>


final_class(DevFS, Filesystem);


// Creates an instance of DevFS.
errno_t DevFS_Create(DevFSRef _Nullable * _Nonnull pOutSelf);

#endif /* DevFS_h */
