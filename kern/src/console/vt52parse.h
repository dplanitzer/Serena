//
//  vt52parse.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

struct vt52parse;
#define VT52_MAX_PARAMS 2

typedef enum {
   VT52PARSE_STATE_GROUND = 1,
   VT52PARSE_STATE_ESCAPE = 2,
   VT52PARSE_STATE_COLLECT = 3
} vt52parse_state_t;

typedef enum {
   VT52PARSE_ACTION_ESC_START = 1,
   VT52PARSE_ACTION_ESC_DISPATCH = 2,
   VT52PARSE_ACTION_ESC_DELAY_DISPATCH = 3,
   VT52PARSE_ACTION_COLLECT_BYTE = 4,
   VT52PARSE_ACTION_EXECUTE = 5,
   VT52PARSE_ACTION_PRINT = 6,
   VT52PARSE_ACTION_IGNORE = 7,
   VT52PARSE_ACTION_ERROR = 8
} vt52parse_action_t;

typedef unsigned char vt52_state_change_t;
typedef void (*vt52parse_callback_t)(void*, vt52parse_action_t, unsigned char);

typedef struct vt52parse {
    vt52parse_state_t      state;
    vt52parse_callback_t   cb;
    void*                  user_data;
    unsigned char          params[VT52_MAX_PARAMS];
    char                   num_params;
    unsigned char          esc_code_seen;
    char                   num_params_to_collect;
    char                   is_atari_extensions_enabled;
} vt52parse_t;

void vt52parse_init(vt52parse_t *parser, vt52parse_callback_t cb, void* user_data);
void vt52parse_reset(vt52parse_t *parser);
void vt52parse_do_state_change(vt52parse_t *parser, unsigned char ch);
