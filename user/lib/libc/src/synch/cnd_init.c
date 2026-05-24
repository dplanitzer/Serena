//
//  cnd_init.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

errno_t cnd_init(cnd_t* _Nonnull self)
{
    atomic_init(&self->seq, 0);    
    return EOK;
}
