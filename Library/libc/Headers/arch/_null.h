//
//  arch/_null.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ARCH_NULL_H
#define __ARCH_NULL_H 1

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#endif /* __ARCH_NULL_H */
