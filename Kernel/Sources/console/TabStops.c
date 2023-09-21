//
//  TabStops.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/21/23.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


// Creates a tab stops object with 'nStops' (initial) tab stops assuming a tab with
// of 'tabWidth' characters.
ErrorCode TabStops_Init(TabStops* _Nonnull pStops, Int nStops, Int tabWidth)
{
    decl_try_err();

    assert(tabWidth >= 0 && tabWidth <= INT8_MAX);

    try(kalloc(sizeof(Int8) * nStops, (Byte**) &pStops->stops));
    pStops->capacity = nStops;
    pStops->count = nStops;

    for (Int i = 0; i < nStops; i++) {
        pStops->stops[i] = i * tabWidth;
    }
    return EOK;

catch:
    return err;
}

void TabStops_Deinit(TabStops* _Nonnull pStops)
{
    kfree((Byte*) pStops->stops);
    pStops->stops = NULL;
}

// Inserts a new tab stop at the absolute X location 'xLoc'. Does nothing if a
// tab stop already exists at this location.
ErrorCode TabStops_InsertStop(TabStops* _Nonnull pStops, Int xLoc)
{
    decl_try_err();
    assert(xLoc >= 0 && xLoc <= INT8_MAX);

    // Find the insertion position
    Int idx = 0;
    for (Int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] >= xLoc) {
            idx = i;
            break;
        }
    }

    if (idx < pStops->count && pStops->stops[idx] == xLoc) {
        return EOK;
    }


    // Insert the new tab stop
    if (pStops->count == pStops->capacity) {
        const Int newCapacity = (pStops->capacity > 0) ? pStops->capacity * 2 : 8;
        Int8* pNewStops;

        try(kalloc(sizeof(Int8) * newCapacity, (Byte**)&pNewStops));
        Bytes_CopyRange((Byte*) pNewStops, (const Byte*) pStops->stops, sizeof(Int8) * pStops->count);
        kfree((Byte*) pStops->stops);
        pStops->stops = pNewStops;
        pStops->capacity = newCapacity;
    }

    if (idx < pStops->count) {
        Bytes_CopyRange(&pStops->stops[idx + 1], &pStops->stops[idx], (pStops->count - idx) * sizeof(Int8));
    }
    pStops->stops[idx] = xLoc;
    pStops->count++;

catch:
    return err;
}

// Removes the tab stop at the given position. Does nothing if the position is
// not associated with a tab stop.
void TabStops_RemoveStop(TabStops* _Nonnull pStops, Int xLoc)
{
    Int idx = -1;

    for (Int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] == xLoc) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        Bytes_CopyRange((Byte*)&pStops->stops[idx], (Byte*)&pStops->stops[idx + 1], (pStops->count - 1 - idx) * sizeof(Int8));
        pStops->count--;
    }
}

// Removes all tab stops.
void TabStops_RemoveAllStops(TabStops* _Nonnull pStops)
{
    pStops->count = 0;
}

// Returns the tab stop following the position 'xLoc'. 'nWidth' - 1 is returned
// if no more tab stop is available.
Int TabStops_GetNextStop(TabStops* pStops, Int xLoc, Int xWidth)
{
    for (Int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] > xLoc) {
            return pStops->stops[i];
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the nth tab stop following the position 'xLoc'. 'nWidth' - 1 is
// returned if no more tab stop is available.
Int TabStops_GetNextNthStop(TabStops* pStops, Int xLoc, Int nth, Int xWidth)
{
    for (Int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] > xLoc) {
            const Int idx = i + (nth - 1);

            return (idx < pStops->count) ? pStops->stops[idx] : __max(xWidth - 1, 0);
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the 'nth' previous tab stop. 0 is returned if no more tab stops are
// available.
Int TabStops_GetPreviousNthStop(TabStops* pStops, Int xLoc, Int nth)
{
    for (Int i = pStops->count - 1; i >= 0; i--) {
        if (pStops->stops[i] < xLoc) {
            const Int idx = i - (__max(nth, 1) - 1);

            return (idx >= 0) ? pStops->stops[idx] : 0;
        }
    }

    return 0;
}