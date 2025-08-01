//
//  machine.c
//  diskimage
//
//  Created by Dietmar Planitzer on 7/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "rc.h"


void rc_retain(volatile ref_count_t* _Nonnull rc)
{
    InterlockedIncrement(rc);
}

bool rc_release(volatile ref_count_t* _Nonnull rc)
{
    if (InterlockedDecrement(rc) == 0) {
        return true;
    }
    else {
        return false;
    }
}

int rc_getcount(volatile ref_count_t* _Nonnull rc)
{
    return *rc;
}
