//
//  UInt64.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "__stddef.h"

uint64_t _divuint64(uint64_t dividend, uint64_t divisor)
{
    int64_t quo;

    __Int64_DivMod(dividend, divisor, &quo, NULL);

    return (uint64_t) quo;
}

uint64_t _divuint64_20(uint64_t dividend, uint64_t divisor)
{
    int64_t quo;
    
    __Int64_DivMod(dividend, divisor, &quo, NULL);
    
    return (uint64_t) quo;
}

uint64_t _moduint64(uint64_t dividend, uint64_t divisor)
{
    int64_t quo, rem;

    __Int64_DivMod(dividend, divisor, &quo, &rem);

    return (uint64_t) rem;
}

uint64_t _moduint64_20(uint64_t dividend, uint64_t divisor)
{
    int64_t quo, rem;
    
    __Int64_DivMod(dividend, divisor, &quo, &rem);
    
    return (uint64_t) rem;
}
