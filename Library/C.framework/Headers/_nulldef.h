//
//  _nulldef.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __NULLDEF_H
#define __NULLDEF_H 1

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#endif /* __NULLDEF_H */
