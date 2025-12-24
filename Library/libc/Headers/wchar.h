//
//  wchar.h
//  libc
//
//  Created by Dietmar Planitzer on 12/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _WCHAR_H
#define _WCHAR_H 1

#if ___STDC_HOSTED__ != 1
#error "not supported in freestanding mode"
#endif

#include <_dmdef.h>
#include <_size.h>
#include <_cmndef.h>
#include <_mbstate.h>
#include <_null.h>
#include <_wchar.h>

__CPP_BEGIN

#define WCHAR_MIN   0
#define WCHAR_MAX   0x3fffff
#define WCHAR_WIDTH 32

typedef int wint_t;

#define WEOF ((wint_t)(-1))

__CPP_END

#endif /* _WCHAR_H */
