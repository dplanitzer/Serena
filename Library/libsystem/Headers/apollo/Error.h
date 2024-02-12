//
//  Error.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ERROR_H
#define _SYS_ERROR_H

#include <abi/_cmndef.h>
#include <abi/_errno.h>
#include <abi/_errtrycatch.h>

__CPP_BEGIN

typedef __errno_t errno_t;
#define EOK __EOK

__CPP_END

#endif /* _SYS_ERROR_H */
