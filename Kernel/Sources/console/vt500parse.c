/*
 * VTParse - an implementation of Paul Williams' DEC compatible state machine parser
 *
 * Author: Joshua Haberman <joshua@reverberate.org>
 *
 * This code is in the public domain.
 */

#include "vt500parse.h"

void vt500parse_init(vt500parse_t *parser, vt500parse_callback_t cb, void* user_data)
{
    vt500parse_reset(parser);
    parser->cb        = cb;
    parser->user_data = user_data;
}

void vt500parse_reset(vt500parse_t *parser)
{
    parser->state                  = VT500PARSE_STATE_GROUND;
    parser->num_intermediate_chars = 0;
    parser->num_params             = 0;
    parser->ignore_flagged         = 0;
}

static void do_action(vt500parse_t *parser, vt500parse_action_t action, unsigned char ch)
{
    /* Some actions we handle internally (like parsing parameters), others
     * we hand to our client for processing */

    switch(action) {
        case VT500PARSE_ACTION_PRINT:
        case VT500PARSE_ACTION_EXECUTE:
        case VT500PARSE_ACTION_HOOK:
        case VT500PARSE_ACTION_PUT:
        case VT500PARSE_ACTION_OSC_START:
        case VT500PARSE_ACTION_OSC_PUT:
        case VT500PARSE_ACTION_OSC_END:
        case VT500PARSE_ACTION_UNHOOK:
        case VT500PARSE_ACTION_CSI_DISPATCH:
        case VT500PARSE_ACTION_ESC_DISPATCH:
            parser->cb(parser->user_data, action, ch);
            break;

        case VT500PARSE_ACTION_IGNORE:
            /* do nothing */
            break;

        case VT500PARSE_ACTION_COLLECT:
        {
            /* Append the character to the intermediate params */
            if(parser->num_intermediate_chars + 1 > MAX_INTERMEDIATE_CHARS)
                parser->ignore_flagged = 1;
            else
                parser->intermediate_chars[parser->num_intermediate_chars++] = ch;

            break;
        }

        case VT500PARSE_ACTION_PARAM:
        {
            /* process the param character */
            if(ch == ';')
            {
                parser->num_params += 1;
                parser->params[parser->num_params-1] = 0;
            }
            else
            {
                /* the character is a digit */
                int current_param;

                if(parser->num_params == 0)
                {
                    parser->num_params = 1;
                    parser->params[0]  = 0;
                }

                current_param = parser->num_params - 1;
                if (parser->params[current_param] < 9999) {
                    parser->params[current_param] *= 10;
                    parser->params[current_param] += (ch - '0');
                }
            }

            break;
        }

        case VT500PARSE_ACTION_CLEAR:
            parser->num_intermediate_chars = 0;
            parser->num_params            = 0;
            parser->ignore_flagged        = 0;
            break;

        default:
            parser->cb(parser->user_data, VT500PARSE_ACTION_ERROR, 0);
    }
}

void vt500parse_do_state_change(vt500parse_t *parser, unsigned char ch)
{
    /* A state change is an action and/or a new state to transition to. */
    const state_change_t change = VT500_STATE_TABLE[parser->state-1][ch];
    vt500parse_state_t  new_state = STATE(change);
    vt500parse_action_t action    = ACTION(change);


    if(new_state)
    {
        /* Perform up to three actions:
         *   1. the exit action of the old state
         *   2. the action associated with the transition
         *   3. the entry action of the new state
         */

        vt500parse_action_t exit_action = VT500_EXIT_ACTIONS[parser->state-1];
        vt500parse_action_t entry_action = VT500_ENTRY_ACTIONS[new_state-1];

        if(exit_action)
            do_action(parser, exit_action, 0);

        if(action)
            do_action(parser, action, ch);

        if(entry_action)
            do_action(parser, entry_action, 0);

        parser->state = new_state;
    }
    else
    {
        do_action(parser, action, ch);
    }
}
