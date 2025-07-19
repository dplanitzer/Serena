//
//  vcpu_stack.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "vcpu_stack.h"
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kern/limits.h>
#include <machine/Platform.h>


// Initializes an execution stack struct. The execution stack is empty by default
// and you need to call vcpu_stack_setmaxsize() to allocate the stack with
// the required size.
// \param pStack the stack object (filled in on return)
void vcpu_stack_Init(vcpu_stack_t* _Nonnull self)
{
    self->base = NULL;
    self->size = 0;
}

// Sets the size of the execution stack to the given size. Does not attempt to preserve
// the content of the existing stack.
errno_t vcpu_stack_setmaxsize(vcpu_stack_t* _Nullable self, size_t size)
{
    decl_try_err();
    const size_t newSize = (size > 0) ? __Ceil_PowerOf2(size, STACK_ALIGNMENT) : 0;
    
    if (self->size != newSize) {
        void* nsp = NULL;

        if (newSize > 0) {
            err = kalloc(newSize, (void**) &nsp);
            if (err != EOK) {
                return err;
            }
        }

        kfree(self->base);
        self->base = nsp;
        self->size = newSize;
    }
    
    return EOK;
}

// Frees the given stack.
// \param pStack the stack
void vcpu_stack_destroy(vcpu_stack_t* _Nullable self)
{
    if (self) {
        kfree(self->base);
        self->base = NULL;
        self->size = 0;
    }
}
