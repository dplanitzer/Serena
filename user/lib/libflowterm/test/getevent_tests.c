//
//  getevent_tests.c
//  libflowterm Tests
//
//  Created by Dietmar Planitzer on 8/25/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <flowterm.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>


void getevent_test(int argc, char *argv[])
{
    bool done = false;

    ft_init();
    printf("Exit with 'q'\n\n");

    while (!done) {
        ft_event_t evt;

        if (ft_getevent(FT_ANY, 0, &evt) < 0) {
            printf("EOF with errno: %d\n", errno);
            break;
        }


        switch (evt.type) {
            case FT_EVT_NULL:
                puts("NULL");
                break;

            case FT_EVT_CHAR: {
                if (evt.data.character.unicode == 'q') {
                    done = true;
                    break;
                }

                const char ch = (isprint(evt.data.character.unicode)) ? evt.data.character.unicode : '!';
                printf("CHAR: '%c' - 0x%x\n", ch, evt.data.character.unicode);
                break;
            }

            case FT_EVT_MOUSE_DOWN:
                printf("MOUSE DOWN: (%d, %d) - %x\n", evt.data.mouse.x, evt.data.mouse.y, evt.data.mouse.buttons);
                break;

            case FT_EVT_MOUSE_UP:
                printf("MOUSE DOWN: (%d, %d) - %x\n", evt.data.mouse.x, evt.data.mouse.y, evt.data.mouse.buttons);
                break;

            case FT_EVT_SCROLL_WHEEL:
                printf("SCROLL WHEEL: (%d, %d)\n", evt.data.scroll_wheel.dx, evt.data.scroll_wheel.dy);
                break;

            case FT_EVT_CURSOR_POSITION:
                printf("CURSOR: (%d, %d)\n", evt.data.cursor.x, evt.data.cursor.y);
                break;

            case FT_EVT_SCREEN_SIZE:
                printf("SCREEN: %d, %d\n", evt.data.screen.x, evt.data.screen.y);
                break;

            default:
                printf("UNKNOWN: %d\n", evt.type);
                break;
        }
    }

    ft_cleanup();
}
