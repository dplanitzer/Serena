//
//  RealtimeClock.h
//  Apollo
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
    Int8    second;         // 0 - 59
    Int8    minute;         // 0 - 59
    Int8    hour;           // 0 - 23
    Int8    dayOfWeek;      // 1 - 7 with Sunday == 1
    Int8    day;            // 1 - 31
    Int8    month;          // 1 - 12
    Int16   year;           // absolute Gregorian year
} GregorianDate;


// 00:00:00 Thursday, 1 January 1970 UTC
extern const GregorianDate  GREGORIAN_DATE_EPOCH;

extern Bool GregorianDate_Equals(const GregorianDate* _Nonnull a, const GregorianDate* _Nonnull b);


OPAQUE_CLASS(RealtimeClock, IOResource);
typedef struct _RealtimeClockMethodTable {
    IOResourceMethodTable   super;
} RealtimeClockMethodTable;


extern ErrorCode RealtimeClock_Create(const SystemDescription* _Nonnull pSysDesc, RealtimeClockRef _Nullable * _Nonnull pOutDriver);

extern ErrorCode RealtimeClock_GetDate(RealtimeClockRef _Nonnull pClock, GregorianDate* _Nonnull pDate);
extern ErrorCode RealtimeClock_SetDate(RealtimeClockRef _Nonnull pClock, const GregorianDate* _Nonnull pDate);

extern ErrorCode RealtimeClock_ReadNonVolatileData(RealtimeClockRef _Nonnull pClock, Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesRead);
extern ErrorCode RealtimeClock_WriteNonVolatileData(RealtimeClockRef _Nonnull pClock, const Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesWritten);

#endif /* RealtimeClock_h */
