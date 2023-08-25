//
//  time.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <time.h>


double difftime(time_t stop_time, time_t start_time)
{
    // XXX needs a __ieeefltsd function
    return 0.0; //(double)(stop_time - start_time);
}
