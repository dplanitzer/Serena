//
//  vcpu_stack.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _VCPU_STACK_H
#define _VCPU_STACK_H 1

#include <kern/errno.h>
#include <kern/types.h>


// A kernel or user execution stack
typedef struct vcpu_stack {
    char* _Nullable base;
    size_t          size;
} vcpu_stack_t;


extern void vcpu_stack_Init(vcpu_stack_t* _Nonnull self);
extern void vcpu_stack_destroy(vcpu_stack_t* _Nullable self);

extern errno_t vcpu_stack_setmaxsize(vcpu_stack_t* _Nullable self, size_t size);

#define vcpu_stack_initialtop(__self) \
    ((size_t)((__self)->base + (__self)->size))

#endif /* _VCPU_STACK_H */
