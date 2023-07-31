//
//  RealtimeClock.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/11/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "RealtimeClock.h"
#include "Bytes.h"
#include "Heap.h"
#include "Platform.h"
#include "VirtualProcessor.h"


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
// RealtimeClock driver
////////////////////////////////////////////////////////////////////////////////
#if 0
#pragma mark -
#endif


RealtimeClock*  gRealtimeClock;

// Checks whether the system has a RTC installed and returns a realtime clock
// object ifg that's the case; otherwise NULL is returned
ErrorCode RealtimeClock_Create(const SystemDescription* _Nonnull pSysDesc, RealtimeClock* _Nullable * _Nonnull pOutDriver)
{
    decl_try_err();
    RealtimeClock* pClock;
    
    try(kalloc_cleared(sizeof(RealtimeClock), (Byte**) &pClock));
    Lock_Init(&pClock->lock);
    
    *pOutDriver = pClock;
    return EOK;
    
catch:
    RealtimeClock_Destroy(pClock);
    *pOutDriver = NULL;
    return err;
}

void RealtimeClock_Destroy(RealtimeClock* _Nullable pClock)
{
    if (pClock) {
        Lock_Deinit(&pClock->lock);
        kfree((Byte*)pClock);
    }
}

// Returns the current Gregorian date & time.
ErrorCode RealtimeClock_GetDate(RealtimeClock* _Nonnull pClock, GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    try(Lock_Lock(&pClock->lock));
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
ErrorCode RealtimeClock_SetDate(RealtimeClock* _Nonnull pClock, const GregorianDate* _Nonnull pDate)
{
    decl_try_err();

    try(Lock_Lock(&pClock->lock));
    // XXX not yet
    Lock_Unlock(&pClock->lock);
    return EOK;

catch:
    return err;
}

// Reads up to 'nBytes' from NVRAM. Returns the actual amount of bytes read.
ErrorCode RealtimeClock_ReadNonVolatileData(RealtimeClock* _Nonnull pClock, ErrorCode* _Nonnull pError, Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesRead)
{
    decl_try_err();
    Int nBytesRead;
    
    try(Lock_Lock(&pClock->lock));
    // XXX not yet
    nBytesRead = 0;
    *pError = ENODEV;
    Lock_Unlock(&pClock->lock);

    *pOutNumBytesRead = nBytesRead;
    return EOK;

catch:
    *pOutNumBytesRead = 0;
    return err;
}

// Writes up to 'nBytes' to NVRAM. Returns the actual amount of data written.
ErrorCode RealtimeClock_WriteNonVolatileData(RealtimeClock* _Nonnull pClock, ErrorCode* _Nonnull pError, const Byte* _Nonnull pBuffer, Int nBytes, Int* _Nonnull pOutNumBytesWritten)
{
    decl_try_err();
    Int nBytesWritten;
    
    try(Lock_Lock(&pClock->lock));
    // XXX not yet
    nBytesWritten = 0;
    *pError = ENODEV;
    Lock_Unlock(&pClock->lock);
    
    *pOutNumBytesWritten = nBytesWritten;
    return EOK;

catch:
    *pOutNumBytesWritten = 0;
    return err;
}
