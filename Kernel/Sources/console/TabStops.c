//
//  TabStops.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/21/23.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"


// Creates a tab stops object with 'nStops' (initial) tab stops assuming a tab with
// of 'tabWidth' characters.
errno_t TabStops_Init(TabStops* _Nonnull pStops, int nStops, int tabWidth)
{
    decl_try_err();

    assert(tabWidth >= 0 && tabWidth <= INT8_MAX);

    try(kalloc(sizeof(int8_t) * nStops, (void**) &pStops->stops));
    pStops->capacity = nStops;
    pStops->count = nStops;

    for (int i = 0; i < nStops; i++) {
        pStops->stops[i] = i * tabWidth;
    }
    return EOK;

catch:
    return err;
}

void TabStops_Deinit(TabStops* _Nonnull pStops)
{
    kfree(pStops->stops);
    pStops->stops = NULL;
}

// Inserts a new tab stop at the absolute X location 'xLoc'. Does nothing if a
// tab stop already exists at this location.
errno_t TabStops_InsertStop(TabStops* _Nonnull pStops, int xLoc)
{
    decl_try_err();
    assert(xLoc >= 0 && xLoc <= INT8_MAX);

    // Find the insertion position
    int idx = 0;
    for (int i = 0; i < pStops->count; i++) {
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
        const int newCapacity = (pStops->capacity > 0) ? pStops->capacity * 2 : 8;
        int8_t* pNewStops;

        try(kalloc(sizeof(int8_t) * newCapacity, (void**) &pNewStops));
        Bytes_CopyRange(pNewStops, pStops->stops, sizeof(int8_t) * pStops->count);
        kfree(pStops->stops);
        pStops->stops = pNewStops;
        pStops->capacity = newCapacity;
    }

    if (idx < pStops->count) {
        Bytes_CopyRange(&pStops->stops[idx + 1], &pStops->stops[idx], (pStops->count - idx) * sizeof(int8_t));
    }
    pStops->stops[idx] = xLoc;
    pStops->count++;

catch:
    return err;
}

// Removes the tab stop at the given position. Does nothing if the position is
// not associated with a tab stop.
void TabStops_RemoveStop(TabStops* _Nonnull pStops, int xLoc)
{
    int idx = -1;

    for (int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] == xLoc) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        Bytes_CopyRange(&pStops->stops[idx], &pStops->stops[idx + 1], (pStops->count - 1 - idx) * sizeof(int8_t));
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
int TabStops_GetNextStop(TabStops* pStops, int xLoc, int xWidth)
{
    for (int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] > xLoc) {
            return pStops->stops[i];
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the nth tab stop following the position 'xLoc'. 'nWidth' - 1 is
// returned if no more tab stop is available.
int TabStops_GetNextNthStop(TabStops* pStops, int xLoc, int nth, int xWidth)
{
    for (int i = 0; i < pStops->count; i++) {
        if (pStops->stops[i] > xLoc) {
            const int idx = i + (nth - 1);

            return (idx < pStops->count) ? pStops->stops[idx] : __max(xWidth - 1, 0);
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the 'nth' previous tab stop. 0 is returned if no more tab stops are
// available.
int TabStops_GetPreviousNthStop(TabStops* pStops, int xLoc, int nth)
{
    for (int i = pStops->count - 1; i >= 0; i--) {
        if (pStops->stops[i] < xLoc) {
            const int idx = i - (__max(nth, 1) - 1);

            return (idx >= 0) ? pStops->stops[idx] : 0;
        }
    }

    return 0;
}