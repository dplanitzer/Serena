//
//  difftime.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


double difftime(time_t time1, time_t time0)
{
    return (double)time1 - (double)time0;
}
