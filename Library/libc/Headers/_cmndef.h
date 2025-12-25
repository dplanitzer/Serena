//
//  _cmndef.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 9/4/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __CMNDEF_H
#define __CMNDEF_H 1

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


// May be applied to a pointer. Marks the pointer as not owning the object it
// points to.
#ifndef _Weak
#define _Weak
#endif


// May be applied to a pointer declaration. The object the pointer points to is
// expected to be locked by ie calling mtx_lock() on the object.
#ifndef _Locked
#define _Locked
#endif


// May be applied to a pointer function argument. Marks the argument as taking
// ownership of the provided object reference.
#ifndef _Consuming
#define _Consuming
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

#endif /* __CMNDEF_H */
