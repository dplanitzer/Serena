//
//  inttypes.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/30/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <inttypes.h>
#include <__stddef.h>


intmax_t imaxabs(intmax_t n)
{
    return __abs(n);
}
