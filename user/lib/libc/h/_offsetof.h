//
//  _offsetof.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __OFFSETOF_H
#define __OFFSETOF_H 1

#include <_dmdef.h>

#ifdef __VBCC__
#define offsetof(type, member) __offsetof(type, member)
#else
#define offsetof(type, member) \
    ((__size_t)((char *)&((type *)0)->member - (char *)0))
#endif

#endif /* __OFFSETOF_H */
