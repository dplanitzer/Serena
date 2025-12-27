//
//  divmod64.c
//  libsc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#define NULL ((void*)0)

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);


long long _modsint64_020(long long dividend, long long divisor)
{
    long long quo, rem;
    
    _divmods64(dividend, divisor, &quo, &rem);
    
    return rem;
}

long long _modsint64_060(long long dividend, long long divisor)
{
    long long quo, rem;
    
    _divmods64(dividend, divisor, &quo, &rem);
    
    return rem;
}
