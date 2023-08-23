//
//  UInt64.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Runtime.h"

UInt64 _divuint64(UInt64 dividend, UInt64 divisor)
{
    Int64 quo;

    Int64_DivMod(dividend, divisor, &quo, NULL);

    return (UInt64) quo;
}

UInt64 _divuint64_20(UInt64 dividend, UInt64 divisor)
{
    Int64 quo;
    
    Int64_DivMod(dividend, divisor, &quo, NULL);
    
    return (UInt64) quo;
}

UInt64 _moduint64(UInt64 dividend, UInt64 divisor)
{
    Int64 quo, rem;

    Int64_DivMod(dividend, divisor, &quo, &rem);

    return (UInt64) rem;
}

UInt64 _moduint64_20(UInt64 dividend, UInt64 divisor)
{
    Int64 quo, rem;
    
    Int64_DivMod(dividend, divisor, &quo, &rem);
    
    return (UInt64) rem;
}
