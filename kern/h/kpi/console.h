//
//  kpi/console.h
//  kpi
//
//  Created by Dietmar Planitzer on 6/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_CONSOLE_H
#define _KPI_CONSOLE_H 1

#include <kpi/ioctl.h>


typedef struct con_screen {
    int columns;
    int rows;
} con_screen_t;

// Returns the current screen size of the console.
// con_get_screen(con_screen_t* _Nonnull info)
#define kConsoleCommand_GetScreen   IOResourceCommand(kDriverCommand_SubclassBase + 0)


typedef struct con_cursor {
    int x, y;   // one based
} con_cursor_t;

// Returns the current cursor position.
// con_get_cursor(con_cursor_t* _Nonnull info)
#define kConsoleCommand_GetCursor   IOResourceCommand(kDriverCommand_SubclassBase + 1)

#endif /* _KPI_CONSOLE_H */
