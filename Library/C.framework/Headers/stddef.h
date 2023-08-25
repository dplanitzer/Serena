//
//  stddef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDDEF_H
#define _STDDEF_H 1

typedef unsigned long long size_t;
typedef unsigned long ptrdiff_t;

#define NULL ((void*)0)

#define offset(type, member) __offsetof(type, member)

#endif /* _STDDEF_H */
