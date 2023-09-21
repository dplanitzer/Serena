/*
 * VTParse - an implementation of Paul Williams' DEC compatible state machine parser
 *
 * Author: Joshua Haberman <joshua@reverberate.org>
 *
 * This code is in the public domain.
 */

#ifndef VTPARSE_DOT_H
#define VTPARSE_DOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vtparse_table.h"

#define MAX_INTERMEDIATE_CHARS 2
#define ACTION(state_change) (state_change & 0x0F)
#define STATE(state_change)  (state_change >> 4)

struct vtparse;

typedef void (*vtparse_callback_t)(struct vtparse*, vtparse_action_t, unsigned char);

typedef struct vtparse {
    vtparse_state_t    state;
    vtparse_callback_t cb;
    unsigned char      intermediate_chars[MAX_INTERMEDIATE_CHARS+1];
    short              num_intermediate_chars;
    char               ignore_flagged;
    short              params[16];
    short              num_params;
    void*              user_data;
} vtparse_t;

void vtparse_init(vtparse_t *parser, vtparse_callback_t cb, void* user_data);
void vtparse(vtparse_t *parser, const unsigned char *data, int len);

void vtparse_do_state_change(vtparse_t *parser, state_change_t change, unsigned char ch);

#define vtparse_byte(parser, ch) \
        vtparse_do_state_change(parser, STATE_TABLE[(parser)->state-1][ch], ch)


#ifdef __cplusplus
}
#endif

#endif
