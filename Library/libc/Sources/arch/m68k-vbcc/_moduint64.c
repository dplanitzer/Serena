//
//  _moduint64.c
//  libsc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#define NULL ((void*)0)

extern int _divmods64(long long dividend, long long divisor, long long* quotient, long long* remainder);

unsigned long long _moduint64_020(unsigned long long dividend, unsigned long long divisor)
{
    long long quo, rem;
    
    _divmods64(dividend, divisor, &quo, &rem);
    
    return (unsigned long long) rem;
}
