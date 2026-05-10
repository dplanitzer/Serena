//
//  cnd_init.c
//  libc
//
//  Created by Dietmar Planitzer on 5/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__synch.h"

int cnd_init(cnd_t* _Nonnull self)
{
    self->seq.value = 0;
    self->signature = CND_SIGNATURE;
    
    return 0;
}
