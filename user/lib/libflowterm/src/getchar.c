//
//  getchar.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


int ft_getchar(unsigned int flags)
{
    ft_event_t evt;

    for (;;) {
        if (ft_getevent(FT_ANY, flags, &evt) < 0) {
            return EOF;
        }

        if (evt.type == FT_EVT_CHAR) {
            return evt.data.character.unicode;
        }
        else if (evt.type == FT_EVT_EOF) {
            errno = 0;
            return EOF;
        }

        // Not a character event - discard the event and try again. This is true
        // for blocking and non-blocking modes. Non-blocking mode will continue
        // to spin until no more events are queued or we eventually get a queued
        // character event.
    }
}
