//
//  _size.h
//  libc
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_SIZE_H
#define __SYS_SIZE_H 1

#include <machine/abi/_dmdef.h>

#ifdef __SYSTEM_SHIM__
#include <stddef.h>
#else

typedef __size_t size_t;

#endif /* __SYSTEM_SHIM__ */
#endif /* __SYS_SIZE_H */
