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
#include <System/_errno.h>

__CPP_BEGIN

// XXX make this per VP
extern int _Errno;

#define errno _Errno

__CPP_END

#endif /* _ERRNO_H */
