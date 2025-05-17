//
//  gregorian_calendar.h
//  libsystem
//
//  Created by Dietmar Planitzer on 1/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_GREGORIAN_CALENDAR_H
#define _SYS_GREGORIAN_CALENDAR_H 1

#include <_cmndef.h>
#include <_size.h>
#include <stdbool.h>

__CPP_BEGIN

typedef long days_t;

extern days_t __gc_days_from_civil(int y, unsigned m, unsigned d);
extern void __gc_civil_from_days(days_t z, int *year, unsigned *month, unsigned *day);

extern unsigned __gc_weekday_from_days(days_t z);
extern bool __gc_is_leap(days_t y);

extern unsigned __gc_last_day_of_month_common_year(unsigned m);
extern unsigned __gc_last_day_of_month_leap_year(unsigned m);
extern unsigned __gc_last_day_of_month(days_t y, unsigned m);

extern unsigned __gc_weekday_difference(unsigned x, unsigned y);

extern const char* __gc_abbrev_wday(unsigned z);
extern const char* __gc_abbrev_ymon(unsigned m);

__CPP_END

#endif /* _SYS_GREGORIAN_CALENDAR_H */
