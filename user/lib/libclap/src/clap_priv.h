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

typedef int clap_status_t;


typedef struct clap_param_list {
    clap_param_t* _Nonnull  p;
    int                     count;
} clap_param_list_t;


typedef struct clap_command_entry {
    clap_param_t* _Nonnull  decl;       // Parameter declaring the command
    clap_param_list_t       params;     // Parameters associated with this command
} clap_command_entry_t;


typedef struct clap_command_def {
    clap_command_entry_t * _Nonnull entries;
    int                             entries_count;
    bool                            required;
    bool                            appeared;
} clap_command_def_t;


typedef struct clap {
    const char**        argv;
    int                 argc;
    int                 arg_idx;

    clap_param_list_t   global_params;  // Parameters up to the first End or Command entry
    clap_command_def_t  cmd;            // Definitions of all commands, if commands exist

    clap_param_list_t   cur_params;         // Currently active parameters (global or command-based)
    int                 cur_cmd_idx;        // Index of command in effect; -1 if no command is active
    int                 cur_pos_param_idx;  // Index of the currently active positional parameter; -1 if none is active

    bool                should_interpret_args;  // If true then args are interpreted; if false then they are always assigned to the varargs
    bool                should_terminate;       // Terminates the clap_parse() loop if set to true

    clap_param_t        end_param;      // End parameter used as a sentinel for empty parameter lists
} clap_t;


extern void _clap_help(clap_t* _Nonnull self, const clap_param_t* _Nonnull param);

#ifndef __SERENA__
#define vfiprintf vfprintf
#endif

__CPP_END

#endif /* _CLAP_PRIV_H */
