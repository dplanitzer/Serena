//
//  clapdef.h
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _CLAPDEF_H
#define _CLAPDEF_H 1

#ifdef __SERENA__

#include <_cmndef.h>

#else /* !__SERENA__ */

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

#ifdef __cplusplus
#define __CPP_BEGIN extern "C" {
#else
#define __CPP_BEGIN
#endif

#ifdef __cplusplus
#define __CPP_END }
#else
#define __CPP_END
#endif

#endif

#endif /* _CLAPDEF_H */
