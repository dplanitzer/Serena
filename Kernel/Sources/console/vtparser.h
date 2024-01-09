//
//  vtparser.h
//  Apollo
//
//  Created by Dietmar Planitzer on 1/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "vt52parse.h"
#include "vt500parse.h"

struct vtparser;

typedef enum {
   VTPARSER_MODE_VT52 = 0,
   VTPARSER_MODE_VT52_ATARI,
   VTPARSER_MODE_VT102,
} vtparser_mode_t;

typedef void (*vtparser_do_change_callback_t)(void*, unsigned char);

typedef struct vtparser {
    vt52parse_t                     vt52;
    vt500parse_t                    vt500;
    vtparser_do_change_callback_t   do_change_cb;
    void*                           do_change_parser;
} vtparser_t;

// VT102 is the default mode
void vtparser_init(vtparser_t *parser, vt52parse_callback_t vt52_cb, vt500parse_callback_t vt500_cb, void* user_data);
void vtparser_set_mode(vtparser_t *parser, vtparser_mode_t mode);

#define vtparser_byte(parser, ch) \
        (parser)->do_change_cb((parser)->do_change_parser, ch)
