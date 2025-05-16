//
//  sys/driver.h
//  libc
//
//  Created by Dietmar Planitzer on 4/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_DRIVER_H
#define _SYS_DRIVER_H 1

#include <kern/_cmndef.h>

__CPP_BEGIN

// Driver subclasses define their respective commands based on this number.
#define kDriverCommand_SubclassBase  IOResourceCommand(256)

__CPP_END

#endif /* _SYS_DRIVER_H */
