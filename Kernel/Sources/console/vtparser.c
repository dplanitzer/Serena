//
//  vtparser.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "vtparser.h"

void vtparser_init(vtparser_t *parser, vtparser_callback_t cb, void* user_data)
{
    parser->cb          = cb;
    parser->user_data   = user_data;
    vtparser_set_mode(parser, VTPARSER_MODE_VT102);
}

void vtparser_set_mode(vtparser_t *parser, vtparser_mode_t mode)
{
    switch (mode) {
        case VTPARSER_MODE_VT52:
        case VTPARSER_MODE_VT52_ATARI:
            vt52parse_init(&parser->u.vt52, (vt52parse_callback_t)parser->cb, parser->user_data, (mode == VTPARSER_MODE_VT52_ATARI) ? 1 : 0);
            parser->do_change_cb = (vtparser_do_change_callback_t)vt52parse_do_state_change;
            break;

        default:
            vt500parse_init(&parser->u.vt500, (vt500parse_callback_t)parser->cb, parser->user_data);
            parser->do_change_cb = (vtparser_do_change_callback_t)vt500parse_do_state_change;
            break;
    }
}
