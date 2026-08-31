//
//  events.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"
#include <stdbool.h>
#include <stdlib.h>
#include <ext/queue.h>
#include <serena/fd.h>

struct ft_event_node {
    queue_node_t    node;
    ft_event_t      evt;
};

#define EVENT_NODE_COUNT        48
#define TERMIN_BUFFER_CAPACITY  16
#define CSI_BUFFER_CAPACITY     128

#define STATE_TEXT      0
#define STATE_ESC       1
#define STATE_CSI       2       // Inside an escape sequence payload


static queue_t              g_evt_queue;
static queue_t              g_evt_cache;
static struct ft_event_node g_evt_nodes[EVENT_NODE_COUNT];

static char         g_termin_buffer[TERMIN_BUFFER_CAPACITY];
static signed char  g_termin_buffer_size = 0;
static signed char  g_termin_buffer_index = 0;
static char         g_termin_state = STATE_TEXT;

static char         g_csi_buffer[CSI_BUFFER_CAPACITY + 1];  // +1 for trailing \0
static short        g_csi_buffer_index = 0;
static bool         g_csi_overflowed = false;


void _ft_init_events(void)
{
    for (int i = 0; i < EVENT_NODE_COUNT; i++) {
        queue_add_last(&g_evt_cache, &g_evt_nodes[i].node);
    }
}

static ft_event_t* _Nullable _acquire_event(void)
{
    struct ft_event_node* evt = (struct ft_event_node*)g_evt_cache.first;

    if (evt) {
        _queue_remove_first(&g_evt_cache);
        _queue_add_last(&g_evt_queue, &evt->node);

        return &evt->evt;
    }
    else {
        // drop event
        return NULL;
    }
}

#define _put_char_event(__evt, __ch) \
__evt = _acquire_event(); \
if (__evt) { \
    (__evt)->type = FT_EVT_CHAR; \
    (__evt)->data.character.unicode = __ch; \
}

static bool _parse_csi_params(const char* _Nonnull csi, int paramsCount, int* _Nonnull params)
{
    char* ep = NULL;

    for (int i = 0; i < paramsCount; i++) {
        params[i] = (int)strtol(csi, &ep, 10);
        if ((paramsCount > 1) && (i < (paramsCount - 1)) && (*ep != ';')) {
            return false;
        }
        csi = (const char*)(ep + 1);
    }

    return true;
}

// First character in 'csi' is teh first character after the CSI prefix: '\e['. 
static ft_event_t* _Nullable _put_csi_event(const char* _Nonnull csi, short len)
{
#define TILDE_CSI_CODE_TABLE_SIZE   34
    static const unsigned short g_tilde_csi_code_to_pua_code[TILDE_CSI_CODE_TABLE_SIZE] = {
        FT_CHAR_HOME,
        FT_CHAR_INSERT,
        FT_CHAR_DELETE,
        0,
        FT_CHAR_PAGE_UP,
        FT_CHAR_PAGE_DOWN,
        0,
        FT_CHAR_END,
        0,
        0,
        FT_CHAR_FKEY_F1,
        FT_CHAR_FKEY_F2,
        FT_CHAR_FKEY_F3,
        FT_CHAR_FKEY_F4,
        FT_CHAR_FKEY_F5,
        0,
        FT_CHAR_FKEY_F6,
        FT_CHAR_FKEY_F7,
        FT_CHAR_FKEY_F8,
        FT_CHAR_FKEY_F9,
        FT_CHAR_FKEY_F10,
        0,
        FT_CHAR_FKEY_F11,
        FT_CHAR_FKEY_F12,
        FT_CHAR_FKEY_F13,
        FT_CHAR_FKEY_F14,
        0,
        FT_CHAR_FKEY_F15,
        FT_CHAR_FKEY_F16,
        0,
        FT_CHAR_FKEY_F17,
        FT_CHAR_FKEY_F18,
        FT_CHAR_FKEY_F19,
        FT_CHAR_FKEY_F20,
    };
    int p[3];

    if (len == 0) {
        return NULL;
    }

    ft_event_t* evt = _acquire_event();
    if (evt == NULL) {
        return NULL;
    }
    //XXX We mark the CSI event as invalid/broken CSI by default. Find a better
    //XXX way to handle these kind of situations 
    evt->type = FT_EVT_CHAR;
    evt->data.character.unicode = 0;

    switch (csi[0]) {
        case 'A':   // "\e[A"
            evt->type = FT_EVT_CHAR;
            evt->data.character.unicode = FT_CHAR_CURSOR_UP;
            break;

        case 'B':   // "\e[B"
            evt->type = FT_EVT_CHAR;
            evt->data.character.unicode = FT_CHAR_CURSOR_DOWN;
            break;

        case 'D':   // "\e[D"
            evt->type = FT_EVT_CHAR;
            evt->data.character.unicode = FT_CHAR_CURSOR_LEFT;
            break;

        case 'C':   // "\e[C"
            evt->type = FT_EVT_CHAR;
            evt->data.character.unicode = FT_CHAR_CURSOR_RIGHT;
            break;

        case 'M':   // "\e [ M Cb Cx Cy"        Legacy Mouse Event
            if (len == 4) {
                evt->type = FT_EVT_MOUSE_DOWN;
                evt->data.mouse.modifiers = 0;
                evt->data.mouse.button_number = csi[1] - 32;
                evt->data.mouse.x = csi[2] - 32;
                evt->data.mouse.y = csi[3] - 32;
            }
            break;

        case '<':   // "\e [ < Cb ; Cx ; Cy (M|m)"  SGR Mouse Event
            if (_parse_csi_params(csi + 1, 3, p)) {
                const bool isButtonPressed = (csi[len - 1] == 'M');
                const bool hasMotion = (p[0] & 32);

                if (hasMotion) {
                    evt->type = (isButtonPressed) ? FT_EVT_MOUSE_DRAG : FT_EVT_MOUSE_MOVE;
                } else {
                    evt->type = (isButtonPressed) ? FT_EVT_MOUSE_DOWN : FT_EVT_MOUSE_UP;
                }
                evt->data.mouse.modifiers = p[0] & FT_MODIFIER_MASK;
                evt->data.mouse.button_number = p[0] & ~FT_MODIFIER_MASK;
                evt->data.mouse.x = p[1];
                evt->data.mouse.y = p[2];
            }
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            switch (csi[len - 1]) {
                case '~':   // "\e[ INTEGER ~"
                    evt->type = FT_EVT_CHAR;
                    if (_parse_csi_params(csi, 1, p) && p[0] >= 0 && p[0] < TILDE_CSI_CODE_TABLE_SIZE) {
                        evt->data.character.unicode = g_tilde_csi_code_to_pua_code[p[0] - 1];
                    }
                    break;

                case 'R':   // "\e[row ; col R"     Cursor Position
                    evt->type = _FT_EVT_CURSOR_POSITION;
                    if (_parse_csi_params(csi, 2, p)) {
                        evt->data.cursor.y = p[0];
                        evt->data.cursor.x = p[1];
                    }
                    break;
            }
            break;
    }

    return evt;
}

#define _append_csi_char(__ch) \
if (g_csi_buffer_index < CSI_BUFFER_CAPACITY) { \
    g_csi_buffer[g_csi_buffer_index++] = __ch; \
} else { \
    g_csi_overflowed = true; \
}


static int _read_lookahead_byte(void)
{
    //XXX should really use a small timeout instead so that lookahead can
    // work correctly with serial terminals. We're currently assuming that
    // we're always talking to the local console which inserts a CSI atomically.
    // Timeout e.g. 20ms to 100ms or so.  
    const fd_flags_t oflags = fd_flags(__ft_termin_fd);
    fd_setflags(__ft_termin_fd, FD_FOP_ADD, O_NONBLOCK);

    char by;
    const size_t nBytesRead = fd_read(__ft_termin_fd, &by, 1);
    errno = 0;

    const int r = fd_setflags(__ft_termin_fd, FD_FOP_REPLACE, oflags);

    if (nBytesRead == -1) {
        return EOF;
    }
    else {
        return by;
    }
}

static int _fill_buffer(void)
{
    const ssize_t nBytesRead = fd_read(__ft_termin_fd, g_termin_buffer, TERMIN_BUFFER_CAPACITY);

    if (nBytesRead < 0) {
        return EOF;
    }

    g_termin_buffer_index = 0;
    g_termin_buffer_size = nBytesRead;

    return 0;
}

static int _wait_event(void)
{
    for (;;) {
        ft_event_t* evt = NULL;

        if ((g_termin_buffer_index == g_termin_buffer_size) && (g_termin_state != STATE_ESC)) {
            if (_fill_buffer() < 0) {
                return EOF;
            }
        }


        switch (g_termin_state) {
            case STATE_TEXT: {
                const char ch = g_termin_buffer[g_termin_buffer_index++];

                if (ch == '\033') {
                    g_termin_state = STATE_ESC;
                }
                else {
                    _put_char_event(evt, ch);
                }
                break;
            }

            case STATE_ESC: {
                int ch;

                if (g_termin_buffer_index == g_termin_buffer_size) {
                    ch = _read_lookahead_byte();
                }
                else {
                    ch = g_termin_buffer[g_termin_buffer_index++];
                }


                if (ch == '[') {
                    g_termin_state = STATE_CSI;
                }
                else {
                    // not a CSI
                    _put_char_event(evt, (ch == EOF) ? '\033' : ch);
                    g_termin_state = STATE_TEXT;
                }
                break;
            }

            case STATE_CSI: {
                const char ch = g_termin_buffer[g_termin_buffer_index++];

                _append_csi_char(ch);

                if (ch >= '@' && ch <= '~') {
                    g_csi_buffer[g_csi_buffer_index] = '\0';
                    if (!g_csi_overflowed) {
                        evt = _put_csi_event(g_csi_buffer, g_csi_buffer_index);
                    }
                    g_csi_buffer_index = 0;
                    g_csi_overflowed = false;

                    g_termin_state = STATE_TEXT;
                }
                break;
            }
        }


        if (evt) {
            break;
        }
    }

    return 0;
}


int ft_getevent(unsigned int mask, unsigned int flags, ft_event_t* _Nonnull pOutEvent)
{
    _ft_config_termin(flags);

    for (;;) {
        struct ft_event_node* the_evt = NULL;
        struct ft_event_node* prev_evt = NULL;

        queue_for_each(&g_evt_queue, struct ft_event_node, it, {
            if (((1u << (unsigned int)it->evt.type) & mask) != 0) {
                the_evt = it;
                break;
            }

            prev_evt = it;
        });


        if (the_evt) {
            *pOutEvent = the_evt->evt;
            
            if ((flags & FT_PEEK) == 0) {
                queue_remove(&g_evt_queue, &prev_evt->node, &the_evt->node);
                _queue_add_first(&g_evt_cache, &the_evt->node);
            }
            return 0;
        }


        if (_wait_event() < 0) {
            return EOF;
        }
    }
}
