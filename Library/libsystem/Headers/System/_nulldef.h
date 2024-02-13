//
//  _nulldef.h
//  libsystem
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_NULLDEF_H
#define __SYS_NULLDEF_H 1

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif


#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void*)0)
#endif
#endif

#endif /* __SYS_NULLDEF_H */
