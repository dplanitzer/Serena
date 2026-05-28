//
//  SerenaFS.h
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef SerenaFS_h
#define SerenaFS_h

#include <filesystem/Filesystem.h>


final_class(SerenaFS, Filesystem);


// Creates an instance of SerenaFS.
extern errno_t SerenaFS_Create(FSContainerRef _Nonnull pContainer, SerenaFSRef _Nullable * _Nonnull pOutSelf);

#endif /* SerenaFS_h */
