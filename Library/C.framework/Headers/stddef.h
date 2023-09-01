//
//  stddef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

#include <_dmdef.h>
#include <_nulldef.h>
#include <_sizedef.h>


typedef __ptrdiff_t ptrdiff_t;


#define offset(type, member) __offsetof(type, member)

#endif /* _STDDEF_H */
