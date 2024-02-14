//
//  stddef.h
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <System/abi/_dmdef.h>
#include <System/abi/_sizedef.h>
#include <System/_cmndef.h>
#include <System/_nulldef.h>
#include <System/_offsetof.h>

__CPP_BEGIN

typedef __ptrdiff_t ptrdiff_t;
typedef unsigned short wchar_t;

__CPP_END

#endif /* _STDDEF_H */
