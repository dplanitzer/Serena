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

static ft_event_t* _Nullable _put_csi_event(const char* _Nonnull csi)
{
    //XXX parse CSI and create a suitable event
    return NULL;
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
                        evt = _put_csi_event(g_csi_buffer);
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
