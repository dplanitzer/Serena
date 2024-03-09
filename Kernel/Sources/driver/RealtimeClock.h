//
//  RealtimeClock.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef RealtimeClock_h
#define RealtimeClock_h

#include <klib/klib.h>
#include "IOResource.h"
#include "SystemDescription.h"


// A specific date in the Gregorian calendar
typedef struct _GregorianDate {
    int8_t  second;         // 0 - 59
    int8_t  minute;         // 0 - 59
    int8_t  hour;           // 0 - 23
    int8_t  dayOfWeek;      // 1 - 7 with Sunday == 1
    int8_t  day;            // 1 - 31
    int8_t  month;          // 1 - 12
    int16_t year;           // absolute Gregorian year
} GregorianDate;


// 00:00:00 Thursday, 1 January 1970 UTC
extern const GregorianDate  GREGORIAN_DATE_EPOCH;

extern bool GregorianDate_Equals(const GregorianDate* _Nonnull a, const GregorianDate* _Nonnull b);


OPAQUE_CLASS(RealtimeClock, IOResource);
typedef struct _RealtimeClockMethodTable {
    IOResourceMethodTable   super;
} RealtimeClockMethodTable;


extern errno_t RealtimeClock_Create(const SystemDescription* _Nonnull pSysDesc, RealtimeClockRef _Nullable * _Nonnull pOutDriver);

extern errno_t RealtimeClock_GetDate(RealtimeClockRef _Nonnull pClock, GregorianDate* _Nonnull pDate);
extern errno_t RealtimeClock_SetDate(RealtimeClockRef _Nonnull pClock, const GregorianDate* _Nonnull pDate);

extern errno_t RealtimeClock_ReadNonVolatileData(RealtimeClockRef _Nonnull pClock, void* _Nonnull pBuffer, int nBytes, int* _Nonnull pOutNumBytesRead);
extern errno_t RealtimeClock_WriteNonVolatileData(RealtimeClockRef _Nonnull pClock, const void* _Nonnull pBuffer, int nBytes, int* _Nonnull pOutNumBytesWritten);

#endif /* RealtimeClock_h */
