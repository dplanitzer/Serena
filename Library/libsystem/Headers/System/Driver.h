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

// Returns information about a disk drive.
// get_info(DiskInfo* _Nonnull pOutInfo)
#define kDriverCommand_SubclassBase  IOResourceCommand(256)

__CPP_END

#endif /* _SYS_DRIVER_H */
