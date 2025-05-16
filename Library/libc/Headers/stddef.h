//
//  stddef.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <machine/abi/_dmdef.h>
#include <machine/abi/_size.h>
#include <kern/_cmndef.h>
#include <kern/_null.h>
#include <kern/_offsetof.h>

__CPP_BEGIN

typedef __ptrdiff_t ptrdiff_t;
typedef unsigned short wchar_t;

__CPP_END

#endif /* _STDDEF_H */
