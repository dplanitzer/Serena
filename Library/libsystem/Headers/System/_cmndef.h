//
//  _cmndef.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/4/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __SYS_CMNDEF_H
#define __SYS_CMNDEF_H 1

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


#ifndef _Restrict
#if _MSC_VER && !__INTEL_COMPILER
// MSVC doesn't support C99
#define _Restrict
#else
#define _Restrict restrict
#endif
#endif


#ifndef _Weak
#define _Weak
#endif


#ifndef _Locked
#define _Locked
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

#endif /* __SYS_CMNDEF_H */
