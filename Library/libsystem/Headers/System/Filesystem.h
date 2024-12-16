//
//  Filesystem.h
//  libsystem
//
//  Created by Dietmar Planitzer on 12/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_FILESYSTEM_H
#define _SYS_FILESYSTEM_H 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <System/Types.h>

__CPP_BEGIN

enum UnmountOptions {
    kUnmount_Forced = 1,    // Force the unmount even if there are still files open
};


#if !defined(__KERNEL__)

// Mounts the filesystem stored in the container at 'containerPath' at the
// directory 'atDirPath'. 'params' are optional mount parameters that are passed
// to the filesystem to mount.
extern errno_t Mount(const char* _Nonnull containerPath, const char* _Nonnull atDirPath, const void* _Nullable params, size_t paramsSize);

// Unmounts the filesystem mounted at the directory 'atDirPath'.
extern errno_t Unmount(const char* _Nonnull atDirPath, uint32_t options);

#endif /* __KERNEL__ */

__CPP_END

#endif /* _SYS_FILESYSTEM_H */
