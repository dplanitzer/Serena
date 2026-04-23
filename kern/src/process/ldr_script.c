//
//  ldr_script.c
//  kernel
//
//  Created by Dietmar Planitzer on 4/21/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "ldr_script.h"
#include <string.h>


static char* _Nonnull _skip_ws(char* _Nonnull p)
{
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    return p;
}

static char* _Nonnull _scan_text(char* _Nonnull p)
{
    for (;;) {
        if (*p == '\0' || *p == ' ' || *p == '\t' || *p == '\n') {
            break;
        }

        p++;
    }

    return p;
}

// Expected syntax:
// #![WS]interp_path[WS][interp_arg]WS NL
//
// WS: [' ', '\t']
// NL: ['\n']
// We required that the #! line ends in whitespace to ensure that we do not
// accidentally cut short the interpreter path or argument because we only read
// the first PROC_IMG_PREFIX_SIZE bytes into memory.
errno_t ldr_script_load(proc_img_t* _Nonnull self)
{
    decl_try_err();
    char* p = self->prefix_buf;
    char* interp = NULL;
    char* interp_arg = NULL;

    if (self->prefix_length < 2 || p[0] != '#' || p[1] != '!') {
        return ENOEXEC;
    }
    if (self->nesting_level >= 1) {
        return ENOEXEC;
    }
    self->nesting_level++;

    
    p[self->prefix_length - 1] = '\0'; 
    p += 2;
    p = _skip_ws(p);
    if (*p == '\0' || *p == '\n') {
        return ENOEXEC;
    }


    // Pick up the interpreter path
    interp = p;
    p = _scan_text(p);
    if (*p == '\0') {
        return ENOEXEC;
    }

    bool has_nl = (*p == '\n');
    *p++ = '\0';


    // Parse the optional interpret argument
    if (!has_nl) {
        char* pp = _skip_ws(p);
        char* ep = _scan_text(pp);

        p = _skip_ws(ep);
        if (*p != '\n') {
            return ENOEXEC;
        }

        if (ep != pp) {
            interp_arg = pp;
        }
    }

    proc_img_replace_arg0(self, interp, interp_arg, self->orig_path);
    err = proc_img_open_file(self, interp);

    return (err == EOK) ? ENOEXEC : err;
}
