//
//  _offsetof.h
//  libsystem
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_OFFSETOF_H
#define __ABI_OFFSETOF_H 1

#include <abi/_dmdef.h>

#ifdef __VBCC__
#define offsetof(type, member) __offsetof(type, member)
#else
#define offsetof(type, member) \
    ((__size_t)((char *)&((type *)0)->member - (char *)0))
#endif

#endif /* __ABI_OFFSETOF_H */
