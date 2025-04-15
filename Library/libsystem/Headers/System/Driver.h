//
//  Driver.h
//  libsystem
//
//  Created by Dietmar Planitzer on 4/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DRIVER_H
#define _SYS_DRIVER_H 1

#include <System/IOChannel.h>

__CPP_BEGIN

// Returns the driver's canonical name. This is the path relative to /dev that
// represents the driver entry in the hw tree.
// get_canonical_name(size_t bufSize, char* _Nonnull buf)
#define kDriverCommand_GetCanonicalName IOResourceCommand(0)

// Returns information about a disk drive.
// get_info(DiskInfo* _Nonnull pOutInfo)
#define kDriverCommand_SubclassBase  IOResourceCommand(256)

__CPP_END

#endif /* _SYS_DRIVER_H */
