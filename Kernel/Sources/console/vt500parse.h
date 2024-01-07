/*
 * VTParse - an implementation of Paul Williams' DEC compatible state machine parser
 *
 * Author: Joshua Haberman <joshua@reverberate.org>
 *
 * This code is in the public domain.
 */

#ifndef VT500PARSE_DOT_H
#define VT500PARSE_DOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vt500parse_table.h"

#define MAX_INTERMEDIATE_CHARS 2
#define ACTION(state_change) (state_change & 0x0F)
#define STATE(state_change)  (state_change >> 4)

struct vt500parse;

typedef void (*vt500parse_callback_t)(struct vt500parse*, vt500parse_action_t, unsigned char);

// <https://vt100.net/emu/dec_ansi_parser>
typedef struct vt500parse {
    vt500parse_state_t      state;
    vt500parse_callback_t   cb;
    void*                   user_data;
    unsigned char           intermediate_chars[MAX_INTERMEDIATE_CHARS+1];
    short                   num_intermediate_chars;
    short                   params[16];
    short                   num_params;
    char                    ignore_flagged;
} vt500parse_t;

void vt500parse_init(vt500parse_t *parser, vt500parse_callback_t cb, void* user_data);

void vt500parse_do_state_change(vt500parse_t *parser, state_change_t change, unsigned char ch);

#define vt500parse_byte(parser, ch) \
        vt500parse_do_state_change(parser, VT500_STATE_TABLE[(parser)->state-1][ch], ch)


#ifdef __cplusplus
}
#endif

#endif
