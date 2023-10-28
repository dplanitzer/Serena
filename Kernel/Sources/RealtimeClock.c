//
//  RealtimeClock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "RealtimeClock.h"
#include "Lock.h"
#include "Platform.h"


// The realtime clock
CLASS_IVARS(RealtimeClock, IOResource,
    Int     type;
    Lock    lock;
    // XXX not fully implemented yet
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Gregorian Date

// 00:00:00 Thursday, 1 January 1970 UTC
const GregorianDate GREGORIAN_DATE_EPOCH = { 0, 0, 0, 5, 1, 1, 1970 };

Bool GregorianDate_Equals(const GregorianDate* _Nonnull a, const GregorianDate* _Nonnull b)
{
    return a->second == b->second
        && a->minute == b->minute
        && a->hour == b->hour
        && a->day == b->day
        && a->month == b->month
        && a->year == b->year
        && a->dayOfWeek == b->dayOfWeek;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: RealtimeClock

// Checks whether the system has a RTC installed and returns a realtime clock
// object ifg that's the case; otherwise NULL is returned
ErrorCode RealtimeClock_Create(const SystemDescription* _Nonnull pSysDesc, RealtimeClockRef _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    RealtimeClockRef pClock;
    
    try(Object_Create(RealtimeClock, &pClock));
    Lock_Init(&pClock->lock);
    
    *pOutDriver = pClock;
    return EOK;
    
catch:
    Object_Release(pClock);
    *pOutDriver = NULL;
    return err;
}

static void RealtimeClock_deinit(RealtimeClockRef _Nonnull pClock)
{
    Lock_Deinit(&pClock->lock);
}

// Returns the current Gregorian date & time.
ErrorCode RealtimeClock_GetDate(RealtimeClockRef _Nonnull pClock, GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    Lock_Lock(&pClock->lock);
    pDate->second = 0;
    pDate->minute = 0;
    pDate->hour = 0;
    pDate->dayOfWeek = 1;
    pDate->day = 1;
    pDate->month = 1;
    pDate->year = 2022;
    // XXX do the real thing
    Lock_Unlock(&pClock->lock);
    return EOK;

catch:
    return err;
}

// Sets the current Gregorian date & time and makes sure that the clock is
// running.
ErrorCode RealtimeClock_SetDate(RealtimeClockRef _Nonnull pClock, const GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    Lock_Lock(&pClock->lock);
    // XXX not yet
    Lock_Unlock(&pClock->lock);
    return EOK;

catch:
    return err;
}

// Reads up to 'nBytes' from NVRAM. Returns the actual amount of bytes read.
ErrorCode RealtimeClock_ReadNonVolatileData(RealtimeClockRef _Nonnull pClock, Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesRead)
{
    decl_try_err();
    Int nBytesRead;
    
    Lock_Lock(&pClock->lock);
    // XXX not yet
    nBytesRead = 0;
    Lock_Unlock(&pClock->lock);

    *pOutNumBytesRead = nBytesRead;
    return EOK;

catch:
    *pOutNumBytesRead = 0;
    return err;
}

// Writes up to 'nBytes' to NVRAM. Returns the actual amount of data written.
ErrorCode RealtimeClock_WriteNonVolatileData(RealtimeClockRef _Nonnull pClock, const Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesWritten)
{
    decl_try_err();
    Int nBytesWritten;
    
    Lock_Lock(&pClock->lock);
    // XXX not yet
    nBytesWritten = 0;
    Lock_Unlock(&pClock->lock);
    
    *pOutNumBytesWritten = nBytesWritten;
    return EOK;

catch:
    *pOutNumBytesWritten = 0;
    return err;
}


CLASS_METHODS(RealtimeClock, IOResource,
OVERRIDE_METHOD_IMPL(deinit, RealtimeClock, Object)
);
