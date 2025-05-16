//
//  errno.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ERRNO_H
#define _ERRNO_H 1

#include <System/_cmndef.h>
#include <kern/_errno.h>

__CPP_BEGIN

extern _Errno_t* _Nonnull __vcpuerrno(void);

#define errno (*__vcpuerrno())

__CPP_END

#endif /* _ERRNO_H */
