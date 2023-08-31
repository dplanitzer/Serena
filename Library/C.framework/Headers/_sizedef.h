//
//  _sizedef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SIZEDEF_H
#define __SIZEDEF_H 1

#define __SIZE_WIDTH 32

#if __SIZE_WIDTH == 32
typedef unsigned long size_t;
#elif __SIZE_WIDTH == 64
typedef unsigned long long size_t;
#else
#error "unknown __SIZE_WIDTH"
#endif

#endif /* __SIZEDEF_H */
