//
//  setjmp.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/26/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _SETJMP_H
#define _SETJMP_H 1

typedef unsigned long   jmp_buf[16];


extern int setjmp(jmp_buf env);
extern void longjmp(jmp_buf env, int status);

#endif /* _SETJMP_H */
