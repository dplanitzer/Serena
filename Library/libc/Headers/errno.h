//
//  errno.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _ERRNO_H
#define _ERRNO_H 1

#include <abi/_cmndef.h>
#include <abi/_errno.h>

__CPP_BEGIN

// XXX make this per VP
extern int errno;

__CPP_END

#endif /* _ERRNO_H */
