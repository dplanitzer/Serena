//
//  stack.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/21/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "stack.h"
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kern/limits.h>
#include <machine/cpu.h>


// Initializes an execution stack struct. The execution stack is empty by default
// and you need to call stk_setmaxsize() to allocate the stack with
// the required size.
// \param pStack the stack object (filled in on return)
void stk_init(stk_t* _Nonnull self)
{
    self->base = NULL;
    self->size = 0;
}

// Frees the given stack.
void stk_destroy(stk_t* _Nullable self)
{
    if (self) {
        kfree(self->base);
        self->base = NULL;
        self->size = 0;
    }
}

// Sets the size of the execution stack to the given size. Does not attempt to preserve
// the content of the existing stack.
errno_t stk_setmaxsize(stk_t* _Nullable self, size_t size)
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
