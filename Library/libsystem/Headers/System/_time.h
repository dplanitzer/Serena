//
//  time.h
//  libsystem
//
//  Created by Dietmar Planitzer on 2/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_TIME_H
#define _SYS_TIME_H 1

#include <System/_cmndef.h>
#include <System/abi/_size.h>

__CPP_BEGIN

#ifndef __time_type_is_defined
#define __time_type_is_defined 1

// Seconds since 00:00, Jan 1st 1970 UTC
typedef long time_t;

#endif  /* __time_type_is_defined */


// Milliseconds
typedef long mseconds_t;

// Microseconds
typedef long useconds_t;


#ifndef __time_spec_is_defined
#define __time_spec_is_defined 1

struct timespec {
    time_t  tv_sec;
    long    tv_nsec;    // 0..<1billion
};

#endif

__CPP_END

#endif /* _SYS_TIME_H */
