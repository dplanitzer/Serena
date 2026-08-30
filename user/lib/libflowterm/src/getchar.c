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

        switch (evt.type) {
            case FT_EVT_CHAR:
                return evt.data.character.unicode;

            case FT_EVT_NULL:
                return 0;
        }

        // discard event and continue waiting if non-blocking isn't set; otherwise
        // treat it as 'no character event available'.
        if ((flags & FT_NONBLOCKING) == FT_NONBLOCKING) {
            return 0;
        }
    }
}
