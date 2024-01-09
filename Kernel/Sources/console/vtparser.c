//
//  vtparser.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "vtparser.h"

void vtparser_init(vtparser_t *parser, vt52parse_callback_t vt52_cb, vt500parse_callback_t vt500_cb, void* user_data)
{
    vt52parse_init(&parser->vt52, vt52_cb, user_data);
    vt500parse_init(&parser->vt500, vt500_cb, user_data);
    parser->do_change_cb = (vtparser_do_change_callback_t)vt500parse_do_state_change;
    parser->do_change_parser = &parser->vt500;
}

void vtparser_set_mode(vtparser_t *parser, vtparser_mode_t mode)
{
    switch (mode) {
        case VTPARSER_MODE_VT52:
        case VTPARSER_MODE_VT52_ATARI:
            vt52parse_reset(&parser->vt52);
            parser->vt52.is_atari_extensions_enabled = (mode == VTPARSER_MODE_VT52_ATARI) ? 1 : 0;
            parser->do_change_cb = (vtparser_do_change_callback_t)vt52parse_do_state_change;
            parser->do_change_parser = &parser->vt52;
            break;

        default:
            vt500parse_reset(&parser->vt500);
            parser->do_change_cb = (vtparser_do_change_callback_t)vt500parse_do_state_change;
            parser->do_change_parser = &parser->vt500;
            break;
    }
}
