//
//  TabStops.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/21/23.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ConsolePriv.h"
#include <string.h>
#include <kern/assert.h>
#include <kern/kalloc.h>


// Creates a tab stops object with 'nStops' (initial) tab stops assuming a tab with
// of 'tabWidth' characters.
errno_t TabStops_Init(TabStops* _Nonnull self, int nStops, int tabWidth)
{
    decl_try_err();

    assert(tabWidth >= 0 && tabWidth <= INT8_MAX);

    try(kalloc(sizeof(int8_t) * nStops, (void**) &self->stops));
    self->capacity = nStops;
    self->count = nStops;

    for (int i = 0; i < nStops; i++) {
        self->stops[i] = i * tabWidth;
    }
    return EOK;

catch:
    return err;
}

void TabStops_Deinit(TabStops* _Nonnull self)
{
    kfree(self->stops);
    self->stops = NULL;
}

// Inserts a new tab stop at the absolute X location 'xLoc'. Does nothing if a
// tab stop already exists at this location.
errno_t TabStops_InsertStop(TabStops* _Nonnull self, int xLoc)
{
    decl_try_err();
    assert(xLoc >= 0 && xLoc <= INT8_MAX);

    // Find the insertion position
    int idx = 0;
    for (int i = 0; i < self->count; i++) {
        if (self->stops[i] >= xLoc) {
            idx = i;
            break;
        }
    }

    if (idx < self->count && self->stops[idx] == xLoc) {
        return EOK;
    }


    // Insert the new tab stop
    if (self->count == self->capacity) {
        const int newCapacity = (self->capacity > 0) ? self->capacity * 2 : 8;
        int8_t* pNewStops;

        try(kalloc(sizeof(int8_t) * newCapacity, (void**) &pNewStops));
        memcpy(pNewStops, self->stops, sizeof(int8_t) * self->count);
        kfree(self->stops);
        self->stops = pNewStops;
        self->capacity = newCapacity;
    }

    if (idx < self->count) {
        memmove(&self->stops[idx + 1], &self->stops[idx], (self->count - idx) * sizeof(int8_t));
    }
    self->stops[idx] = xLoc;
    self->count++;

catch:
    return err;
}

// Removes the tab stop at the given position. Does nothing if the position is
// not associated with a tab stop.
void TabStops_RemoveStop(TabStops* _Nonnull self, int xLoc)
{
    int idx = -1;

    for (int i = 0; i < self->count; i++) {
        if (self->stops[i] == xLoc) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        memmove(&self->stops[idx], &self->stops[idx + 1], (self->count - 1 - idx) * sizeof(int8_t));
        self->count--;
    }
}

// Removes all tab stops.
void TabStops_RemoveAllStops(TabStops* _Nonnull self)
{
    self->count = 0;
}

// Returns the tab stop following the position 'xLoc'. 'nWidth' - 1 is returned
// if no more tab stop is available.
int TabStops_GetNextStop(TabStops* self, int xLoc, int xWidth)
{
    for (int i = 0; i < self->count; i++) {
        if (self->stops[i] > xLoc) {
            return self->stops[i];
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the nth tab stop following the position 'xLoc'. 'nWidth' - 1 is
// returned if no more tab stop is available.
int TabStops_GetNextNthStop(TabStops* self, int xLoc, int nth, int xWidth)
{
    for (int i = 0; i < self->count; i++) {
        if (self->stops[i] > xLoc) {
            const int idx = i + (nth - 1);

            return (idx < self->count) ? self->stops[idx] : __max(xWidth - 1, 0);
        }
    }

    return __max(xWidth - 1, 0);
}

// Returns the 'nth' previous tab stop. 0 is returned if no more tab stops are
// available.
int TabStops_GetPreviousNthStop(TabStops* self, int xLoc, int nth)
{
    for (int i = self->count - 1; i >= 0; i--) {
        if (self->stops[i] < xLoc) {
            const int idx = i - (__max(nth, 1) - 1);

            return (idx >= 0) ? self->stops[idx] : 0;
        }
    }

    return 0;
}