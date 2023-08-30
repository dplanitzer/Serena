//
//  System.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef System_h
#define System_h

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

#ifndef _Noreturn
#define _Noreturn   void
#endif


extern int __write(const char* _Nonnull pString);
extern int __dispatch_async(void* _Nonnull pClosure);
extern int __sleep(int seconds, int nanoseconds);
extern int __alloc_address_space(int nbytes, void* _Nullable * _Nonnull pOutMemPtr);

extern int __spawn_process(void* _Nonnull pUserEntryPoint);
extern _Noreturn __exit(int status);

#endif /* System_h */
