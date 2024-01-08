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

typedef int vtparser_action_t;

typedef void (*vtparser_callback_t)(void*, vtparser_action_t, unsigned char);
typedef void (*vtparser_do_change_callback_t)(void*, unsigned char);

typedef struct vtparser {
    union {
        vt52parse_t     vt52;
        vt500parse_t    vt500;
    }                               u;
    vtparser_do_change_callback_t   do_change_cb;
    vtparser_callback_t             cb;
    void*                           user_data;
} vtparser_t;

void vtparser_init(vtparser_t *parser, vtparser_callback_t cb, void* user_data);
void vtparser_set_mode(vtparser_t *parser, vtparser_mode_t mode);

#define vtparser_byte(parser, ch) \
        (parser)->do_change_cb(parser, ch)
