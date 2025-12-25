//
//  stddef.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <_cmndef.h>
#include <arch/_dmdef.h>
#include <arch/_size.h>
#include <arch/_null.h>
#include <arch/_offsetof.h>
#include <arch/_wchar.h>

__CPP_BEGIN

typedef __ptrdiff_t ptrdiff_t;

__CPP_END

#endif /* _STDDEF_H */
