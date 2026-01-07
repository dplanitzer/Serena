//
//  stack.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _STACK_H
#define _STACK_H 1

#include <ext/try.h>
#include <kern/types.h>


// A kernel or user execution stack
// Stacks grow from high to low address
typedef struct stk {
    char* _Nullable base;
    size_t          size;
} stk_t;


extern void stk_init(stk_t* _Nonnull self);
extern void stk_destroy(stk_t* _Nullable self);

extern errno_t stk_setmaxsize(stk_t* _Nullable self, size_t size);

#define stk_getinitialsp(__self) \
((size_t)((__self)->base + (__self)->size))

#define stk_isvalidsp(__self, __sp) \
((char*)(__sp) >= (__self)->base && (char*)(__sp) < ((__self)->base + (__self)->size))

#endif /* _STACK_H */
