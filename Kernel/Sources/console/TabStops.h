//
//  TabStops.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef TabStops_h
#define TabStops_h

#include <kern/try.h>
#include <kern/types.h>


// Stores up to 255 tab stop positions.
typedef struct TabStops {
    int8_t* stops;
    int     count;
    int     capacity;
} TabStops;


errno_t TabStops_Init(TabStops* _Nonnull self, int nStops, int tabWidth);
void TabStops_Deinit(TabStops* _Nonnull self);
errno_t TabStops_InsertStop(TabStops* _Nonnull self, int xLoc);
void TabStops_RemoveStop(TabStops* _Nonnull self, int xLoc);
void TabStops_RemoveAllStops(TabStops* _Nonnull self);
int TabStops_GetNextStop(TabStops* self, int xLoc, int xWidth);
int TabStops_GetNextNthStop(TabStops* self, int xLoc, int nth, int xWidth);
int TabStops_GetPreviousNthStop(TabStops* self, int xLoc, int nth);

#endif /* TabStops_h */
