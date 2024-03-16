//
//  Types.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef klib_Types_h
#define klib_Types_h

#include <limits.h>
#include <stdarg.h>
#include <System/_cmndef.h>
#include <System/_math.h>
#include <System/abi/_ssize.h>
#include <System/_syslimits.h>
#include <System/Types.h>

extern ssize_t String_Length(const char* _Nonnull pStr);
extern ssize_t String_LengthUpTo(const char* _Nonnull pStr, ssize_t strsz);
extern char* _Nonnull String_CopyUpTo(char* _Nonnull pDst, const char* _Nonnull pSrc, ssize_t count);

#endif /* klib_Types_h */
