//
//  stdnoreturn.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDNORETURN_H
#define _STDNORETURN_H 1

#ifndef _Noreturn
#if defined(__VBCC__) && !defined(__IDE__)
#define _Noreturn __noreturn
#else
#define _Noreturn
#endif
#endif

#ifndef noreturn
#define noreturn _Noreturn
#endif

#endif /* _STDNORETURN_H */
