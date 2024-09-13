//
//  RealtimeClock.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "RealtimeClock.h"
#include <dispatcher/Lock.h>
#include <hal/Platform.h>


// The realtime clock
final_class_ivars(RealtimeClock, Driver,
    int     type;
    Lock    lock;
    // XXX not fully implemented yet
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Gregorian Date

// 00:00:00 Thursday, 1 January 1970 UTC
const GregorianDate GREGORIAN_DATE_EPOCH = { 0, 0, 0, 5, 1, 1, 1970 };

bool GregorianDate_Equals(const GregorianDate* _Nonnull a, const GregorianDate* _Nonnull b)
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
errno_t RealtimeClock_Create(const SystemDescription* _Nonnull pSysDesc, RealtimeClockRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RealtimeClockRef self;
    
    try(Object_Create(RealtimeClock, &self));
    Lock_Init(&self->lock);
    
    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void RealtimeClock_deinit(RealtimeClockRef _Nonnull self)
{
    Lock_Deinit(&self->lock);
}

// Returns the current Gregorian date & time.
errno_t RealtimeClock_GetDate(RealtimeClockRef _Nonnull self, GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    pDate->second = 0;
    pDate->minute = 0;
    pDate->hour = 0;
    pDate->dayOfWeek = 1;
    pDate->day = 1;
    pDate->month = 1;
    pDate->year = 2022;
    // XXX do the real thing
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    return err;
}

// Sets the current Gregorian date & time and makes sure that the clock is
// running.
errno_t RealtimeClock_SetDate(RealtimeClockRef _Nonnull self, const GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    // XXX not yet
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    return err;
}

// Reads up to 'nBytes' from NVRAM. Returns the actual amount of bytes read.
errno_t RealtimeClock_ReadNonVolatileData(RealtimeClockRef _Nonnull self, void* _Nonnull pBuffer, int nBytes, int* _Nonnull pOutNumBytesRead)
{
    decl_try_err();
    int nBytesRead;
    
    Lock_Lock(&self->lock);
    // XXX not yet
    nBytesRead = 0;
    Lock_Unlock(&self->lock);

    *pOutNumBytesRead = nBytesRead;
    return EOK;

catch:
    *pOutNumBytesRead = 0;
    return err;
}

// Writes up to 'nBytes' to NVRAM. Returns the actual amount of data written.
errno_t RealtimeClock_WriteNonVolatileData(RealtimeClockRef _Nonnull self, const void* _Nonnull pBuffer, int nBytes, int* _Nonnull pOutNumBytesWritten)
{
    decl_try_err();
    int nBytesWritten;
    
    Lock_Lock(&self->lock);
    // XXX not yet
    nBytesWritten = 0;
    Lock_Unlock(&self->lock);
    
    *pOutNumBytesWritten = nBytesWritten;
    return EOK;

catch:
    *pOutNumBytesWritten = 0;
    return err;
}


class_func_defs(RealtimeClock, Driver,
override_func_def(deinit, RealtimeClock, Object)
);
