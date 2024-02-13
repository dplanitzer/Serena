//
//  TimeInterval.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef TimeInterval_h
#define TimeInterval_h

#include <System/TimeInterval.h>


extern TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1);

extern TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1);


#define ONE_SECOND_IN_NANOS (1000l * 1000l * 1000l)
#define kQuantums_Infinity      INT32_MAX
#define kQuantums_MinusInfinity INT32_MIN

#endif /* TimeInterval_h */
