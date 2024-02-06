//
//  stddef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <apollo/_cmndef.h>
#include <_dmdef.h>
#include <_nulldef.h>
#include <apollo/_sizedef.h>

__CPP_BEGIN

typedef __ptrdiff_t ptrdiff_t;


#ifdef __VBCC__
#define offsetof(type, member) __offsetof(type, member)
#else
#define offsetof(type, member) \
    ((ByteCount)((char *)&((type *)0)->member - (char *)0))
#endif


__CPP_END

#endif /* _STDDEF_H */
