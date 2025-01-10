//
//  gregorian_calendar.c
//  libc
//
//  Created by Dietmar Planitzer on 1/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "gregorian_calendar.h"


// See:
// <https://howardhinnant.github.io/date_algorithms.html>
// <https://stackoverflow.com/questions/7960318/math-to-convert-seconds-since-1970-into-date-and-vice-versa>


// Returns number of days since civil 1970-01-01.  Negative values indicate
//    days prior to 1970-01-01.
// Preconditions:  y-m-d represents a date in the civil (Gregorian) calendar
//                 m is in [1, 12]
//                 d is in [1, last_day_of_month(y, m)]
//                 y is "approximately" in
//                   [numeric_limits<Int>::min()/366, numeric_limits<Int>::max()/366]
//                 Exact range of validity is:
//                 [civil_from_days(numeric_limits<Int>::min()),
//                  civil_from_days(numeric_limits<Int>::max()-719468)]
days_t __gc_days_from_civil(int y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const days_t era = (y >= 0 ? y : y-399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);             // [0, 399]
    const unsigned doy = (153*(m > 2 ? m-3 : m+9) + 2)/5 + d-1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;     // [0, 146096]
    return era * 146097 + (days_t)doe - 719468;
}


// Returns year/month/day triple in civil calendar
// Preconditions:  z is number of days since 1970-01-01 and is in the range:
//                   [numeric_limits<Int>::min(), numeric_limits<Int>::max()-719468].
void __gc_civil_from_days(days_t z, int *year, unsigned *month, unsigned *day)
{
    z += 719468;
    const days_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);                      // [0, 146096]
    const unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;   // [0, 399]
    const days_t y = (days_t)yoe + era * 400;
    const unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);                 // [0, 365]
    const unsigned mp = (5*doy + 2)/153;                                    // [0, 11]
    const unsigned d = doy - (153*mp+2)/5 + 1;                              // [1, 31]
    const unsigned m = mp < 10 ? mp+3 : mp-9;                               // [1, 12]
    *year = y + (m <= 2);
    *month = m;
    *day = d;
}


// Returns day of week in civil calendar [0, 6] -> [Sun, Sat]
// Preconditions:  z is number of days since 1970-01-01 and is in the range:
//                   [numeric_limits<Int>::min(), numeric_limits<Int>::max()-4].
unsigned __gc_weekday_from_days(days_t z)
{
    return (unsigned)(z >= -4 ? (z+4) % 7 : (z+5) % 7 + 6);
}


// Returns: true if y is a leap year in the civil calendar, else false
bool __gc_is_leap(days_t y)
{
    return  y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of common year
// The result is always in the range [28, 31].
unsigned __gc_last_day_of_month_common_year(unsigned m)
{
    static const unsigned char a[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return a[m-1];
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of leap year
// The result is always in the range [29, 31].
unsigned __gc_last_day_of_month_leap_year(unsigned m)
{
    static const unsigned char a[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return a[m-1];
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of year y
// The result is always in the range [28, 31].
unsigned __gc_last_day_of_month(days_t y, unsigned m)
{
    return m != 2 || !__gc_is_leap(y) ? __gc_last_day_of_month_common_year(m) : 29u;
}


// Preconditions: x <= 6 && y <= 6
// Returns: The number of days from the weekday y to the weekday x.
// The result is always in the range [0, 6].
unsigned __gc_weekday_difference(unsigned x, unsigned y)
{
    x -= y;
    return x <= 6 ? x : x + 7;
}


// Returns the short name of the day
// Preconditions: m is in [0, 6]
const char* __gc_abbrev_wday(unsigned z)
{
    static const char* a[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return a[z];
}


// Returns the short name of the month
// Preconditions: m is in [1, 12]
const char* __gc_abbrev_ymon(unsigned m)
{
    static const char* a[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    return a[m-1];
}
