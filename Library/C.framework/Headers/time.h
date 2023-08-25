//
//  time.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _TIME_H
#define _TIME_H 1

// Seconds since 00:00, Jan 1st 1970 UTC
typedef long time_t;

extern double difftime(time_t stop_time, time_t start_time);

#endif /* _TIME_H */
