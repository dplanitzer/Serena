//
//  clap_priv.h
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _CLAP_PRIV_H
#define _CLAP_PRIV_H 1

#include <clap.h>

__CPP_BEGIN

typedef struct clap_t {
    clap_param_t* _Nonnull  params;
    int                     params_count;
    clap_param_t* _Nullable vararg_param;   // First vararg type parameter in the parameter list; NULL if none exists

    const char**            argv;
    int                     argc;
    int                     arg_idx;

    bool                    should_interpret_args;  // If true then args are interpreted; if false then they are always assigned to the varargs

    clap_param_t * _Nullable * _Nonnull cmds;
    int                                 cmds_count;
    bool                                cmd_required;
    bool                                cmd_appeared;

    const char* _Nullable   cur_label;
    int                     cur_label_len;

    char                    short_label_buffer[3];
} clap_t;


extern void _clap_print_app_name(void);
extern void _clap_help(clap_t* _Nonnull self, const clap_param_t* _Nonnull param);

__CPP_END

#endif /* _CLAP_PRIV_H */
