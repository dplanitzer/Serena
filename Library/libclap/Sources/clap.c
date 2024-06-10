//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap_priv.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


static void clap_analyze_params(clap_t* _Nonnull self, clap_param_t* params);
static void clap_set_params(clap_t* _Nonnull self, int cmdIdx);
static clap_status_t clap_enforce_required_params(clap_t* _Nonnull self);


static void clap_init(clap_t* _Nonnull self, clap_param_t* _Nonnull params, int argc, const char** argv)
{
    memset(self, 0, sizeof(clap_t));
    self->arg_idx = 1;
    self->argc = argc;
    self->argv = argv;
    self->should_interpret_args = true;
    self->end_param.type = clap_type_end;
    self->end_param.long_label = "";
    self->end_param.help = "";

    clap_analyze_params(self, params);
    clap_set_params(self, -1);
}

static void clap_deinit(clap_t* _Nullable self)
{
    if (self) {
        free(self->cmd.entries);
        memset(self, 0, sizeof(clap_t));
    }
}


// Analyzes the provided parameter list and figures out which commands it defines.
// It creates a command table and finds the sections in the parameter list that
// correspond to the global and per-command parameters. 
static void clap_analyze_params(clap_t* _Nonnull self, clap_param_t* _Nonnull params)
{
    clap_param_t* p = params;
    const int max_param_count = INT_MAX;
    bool isCmdRequired = false;
    int nCmds = 0, firstCmdIdx = -1, endIdx = 0;
    int i = 0;

    // Find the index of the first command, number of commands and the end parameter
    for(;;) {
        if (p->type == clap_type_end || i == max_param_count) {
            endIdx = i;
            break;
        }

        if (p->type == clap_type_command) {
            nCmds++;
            if ((p->flags & clap_flag_required) == clap_flag_required) {
                isCmdRequired = true;
            }
            if (firstCmdIdx == -1) {
                firstCmdIdx = i;
            }
        }

        p->flags &= ~clap_flag_appeared;
        p++;
        i++;
    }


    // Collect the global parameter list
    if (firstCmdIdx != 0) {
        self->global_params.p = params;
        self->global_params.count = (firstCmdIdx == -1) ? endIdx : firstCmdIdx;
    }
    else {
        self->global_params.p = &self->end_param;
        self->global_params.count = 0;
    }


    // Collect the command entries
    if (nCmds > 0) {
        self->cmd.entries = calloc(nCmds, sizeof(clap_command_entry_t));
        self->cmd.entries_count = nCmds;
        self->cmd.required = isCmdRequired;

        p = &params[firstCmdIdx]; i = firstCmdIdx;
        for (int cmd_idx = 0; cmd_idx < nCmds; cmd_idx++) {
            int nParams = 0;

            self->cmd.entries[cmd_idx].decl = p;
            self->cmd.entries[cmd_idx].params.p = p + 1;

            p++; i++;
            while (p->type != clap_type_command && p->type != clap_type_end && i < max_param_count) {
                p++; i++; nParams++;
            }

            self->cmd.entries[cmd_idx].params.count = nParams;

            if (nParams == 0) {
                self->cmd.entries[cmd_idx].params.p = &self->end_param;
            }
        }
    }
}

// Sets the currently active parameters and command. Sets the global parameters
// as the active parameters if 'cmdIdx' is -1; otherwise the corresponding command
// and its associated parameters are made active
static void clap_set_params(clap_t* _Nonnull self, int cmdIdx)
{
    if (cmdIdx == -1) {
        self->cur_params = self->global_params;
    }
    else {
        self->cur_params = self->cmd.entries[cmdIdx].params;
    }

    self->cur_cmd_idx = cmdIdx;
    self->cur_pos_param_idx = -1;
}


static void clap_version(clap_t* _Nonnull self, const clap_param_t* _Nonnull param)
{
    if (param->u.text && *param->u.text != '\0') {
        puts(param->u.text);
    }
}


static clap_param_t* _Nullable clap_find_param_by_long_label(clap_t* _Nonnull self, const char* _Nonnull label, size_t label_len)
{
    for (int i = 0; i < self->cur_params.count; i++) {
        clap_param_t* p = &self->cur_params.p[i];

        if (p->long_label && *(p->long_label) != '\0') {
            if (!strncmp(p->long_label, label, label_len) && strlen(p->long_label) == label_len) {
                return p;
            }
        }
    }

    return NULL;
}

static clap_param_t* _Nullable clap_find_param_by_short_label(clap_t* _Nonnull self, char label)
{
    for (int i = 0; i < self->cur_params.count; i++) {
        clap_param_t* p = &self->cur_params.p[i];

        if (p->short_label != '\0' && p->short_label == label) {
            return p;
        }
    }

    return NULL;
}


static clap_status_t clap_update_bool_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq, bool isUnset)
{
    if (eq) {
        clap_param_error(self->argv[0], param, eo, "unexpected value '%s'", eq + 1);
        return EXIT_FAILURE;
    }

    *(bool*)param->value = (isUnset) ? false : true;
    return EXIT_SUCCESS;
}

static clap_status_t clap_update_int_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq)
{
    const char* vptr;

    if (eq) {
        vptr = eq + 1;
    }
    else if (self->arg_idx < self->argc) {
        vptr = self->argv[self->arg_idx++];
    }
    else {
        clap_param_error(self->argv[0], param, eo, "expected an integer");
        return EXIT_FAILURE;
    }


    errno = 0;
    *(int*)param->value = (int)strtol(vptr, NULL, 10);

    if (errno == ERANGE) {
        clap_param_error(self->argv[0], param, eo, "integer %s is out of range", vptr);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

static clap_status_t clap_update_string_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq)
{
    const char* vptr;

    if (eq) {
        vptr = eq + 1;
    }
    else if (self->arg_idx < self->argc) {
        vptr = self->argv[self->arg_idx++];
    }
    else {
        clap_param_error(self->argv[0], param, eo, "expected a string");
        return EXIT_FAILURE;
    }


    *(const char**)param->value = vptr;
    return EXIT_SUCCESS;
}

static clap_status_t clap_update_string_array_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq, bool shouldEndAtLabel)
{
    const char* vptr;
    const char* errmsg = NULL;

    if (eq) {
        errmsg = "expects separate strings";
    }
    else if (self->arg_idx >= self->argc) {
        errmsg = "expected at least one string";
    }

    if (errmsg) {
        clap_param_error(self->argv[0], param, eo, errmsg);
        return EXIT_FAILURE;
    }
    

    // Pre-scan the array to find out how big it is and whether there's a '--'
    // in there which marks the rest of the command line as values
    int startIdx = self->arg_idx;
    int dashDashIdx = -1;
    int i = startIdx;
    bool isDashDashing = false;

    while (i < self->argc) {
        const char* s = self->argv[i];

        if (!isDashDashing && *s == '-') {
            if (s[1] == '-') {
                if (s[2] == '\0') {
                    // -- -> rest of argv is all values
                    dashDashIdx = i;
                    isDashDashing = true;
                }
                else if (shouldEndAtLabel) {
                    // --foo -> end of array
                    break;
                }
            }
            else if (s[1] != '\0' && shouldEndAtLabel) {
                // -foo -> end of array
                break;
            }
            // - -> value
        }

        i++;
    }
    self->arg_idx = i;


    // Possible array configurations
    // [-- a b c]       (string_array storage is argv)
    // [a b c --]       (string_array storage is argv)
    // [ a -- b c]      (string_array storage is malloc'd)
    int endIdx = i;
    if (dashDashIdx == endIdx - 1) {
        endIdx--;
    }
    else if (dashDashIdx == startIdx) {
        startIdx++;
    }


    clap_string_array_t* array = (clap_string_array_t*)param->value;
    array->count = endIdx - startIdx;

    if (array->count == 0) {
        return EXIT_SUCCESS;
    }

    if (dashDashIdx < startIdx || dashDashIdx >= endIdx) {
        // the string array backing store is provided by the argv vector
        array->strings = &self->argv[startIdx];
    }
    else {
        // malloc() the array storage and copy the two halfs (separated by '--') over
        int j = 0;

        array->count--;
        array->strings = malloc(sizeof(char*) * array->count);
        if (array->strings == NULL) {
            return EXIT_FAILURE;
        }

        for (int i = startIdx; i < dashDashIdx; i++) {
            array->strings[j++] = self->argv[i];
        }
        for (int i = dashDashIdx + 1; i < endIdx; i++) {
            array->strings[j++] = self->argv[i];
        }
    }
    return EXIT_SUCCESS;
}

static clap_status_t clap_update_enum_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq)
{
    const char** enum_strs = param->u.enum_strings;
    const char* user_str = "";
    int* iptr = (int*)param->value;
    int i = 0;

    param->value = &user_str;
    const clap_status_t status = clap_update_string_param_value(self, param, eo, eq);
    if (status != 0) {
        return status;
    }

    while (*enum_strs) {
        if (!strcmp(user_str, *enum_strs)) {
            break;
        }

        i++;
        enum_strs++;
    }

    if (*enum_strs) {
        *iptr = i;
        param->value = iptr;
        return EXIT_SUCCESS;
    }
    else {
        clap_param_error(self->argv[0], param, eo, "unknown enum value '%s'", user_str);
        return EXIT_FAILURE;
    }
}

static clap_status_t clap_update_value_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq)
{
    const char* user_str = "";
    void* vptr = param->value;

    param->value = &user_str;
    const clap_status_t status = clap_update_string_param_value(self, param, eo, eq);
    param->value = vptr;

    if (status != 0) {
        return status;
    }

    return param->u.value_func(self->argv[0], param, eo, user_str);
}

static clap_status_t clap_update_named_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, unsigned int eo, const char* _Nullable eq, bool isUnset)
{
    clap_status_t status = 0;

    switch (param->type) {
        case clap_type_boolean:
            // bool (no value)
            status = clap_update_bool_param_value(self, param, eo, eq, isUnset);
            break;

        case clap_type_integer:
            // int (1 value)
            status = clap_update_int_param_value(self, param, eo, eq);
            break;

        case clap_type_string:
            // const char* (1 value)
            status = clap_update_string_param_value(self, param, eo, eq);
            break;

        case clap_type_string_array:
            // [const char*] (multiple values)
            status = clap_update_string_array_param_value(self, param, eo, eq, true);
            break;

        case clap_type_enum:
            // int (1 value)
            status = clap_update_enum_param_value(self, param, eo, eq);
            break;

        case clap_type_value:
            // const char* (1 value)
            status = clap_update_value_param_value(self, param, eo, eq);
            break;

        case clap_type_version:
            clap_version(self, param);
            break;

        case clap_type_help:
            _clap_help(self, param);
            break;

        default:
            abort();
            break;
    }

    param->flags |= clap_flag_appeared;
    if ((param->flags & clap_flag_terminator) == clap_flag_terminator) {
        self->should_terminate = true;
    }
    
    return status;
}

static clap_status_t clap_update_positional_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param)
{
    clap_status_t status = 0;

    switch (param->type) {
        case clap_type_string:
            // const char* (1 value)
            status = clap_update_string_param_value(self, param, 0, NULL);
            break;

        case clap_type_enum:
            // int (1 value)
            status = clap_update_enum_param_value(self, param, 0, NULL);
            break;

        case clap_type_value:
            // const char* (1 value)
            status = clap_update_value_param_value(self, param, 0, NULL);
            break;

        case clap_type_vararg:
            // [const char*] (multiple values)
            status = clap_update_string_array_param_value(self, param, 0, NULL, false);
            break;

        default:
            abort();
            break;
    }

    param->flags |= clap_flag_appeared;
    
    return status;
}


// Parses a long label option like:
// --foo
// --no-foo     (synthesized)
// --foo=x
static clap_status_t clap_parse_long_label_param(clap_t* _Nonnull self)
{
    bool isUnset = false;
    const char* label = self->argv[self->arg_idx++];
    const char* label_to_match = label + 2;

    // Parse --no-foo forms for boolean parameters
    if (!strncmp(label_to_match, "no-", 3)) {
        label_to_match += 3;
        isUnset = true;
    }

    // Parse --foo=x forms for value-accepting parameters
    const char* eq = strchr(label_to_match, '=');
    const size_t label_to_match_len = (eq) ? eq - label_to_match : strlen(label_to_match);
    const size_t label_len = (eq) ? eq - label : strlen(label);

    // Find the parameter for the label
    clap_param_t* param = clap_find_param_by_long_label(self, label_to_match, label_to_match_len);
    if (param == NULL || (param && isUnset && param->type != clap_type_boolean)) {
        clap_error(self->argv[0], "unknown option '%.*s'", label_len, label);
        return EXIT_FAILURE;
    }

    // Update the parameter value
    return clap_update_named_param_value(self, param, clap_eo_long_label, eq, isUnset);
}

// Parses a short label option like:
// -b
// -b=x
// -bcdef
// -bcdef=x
static clap_status_t clap_parse_short_label_param(clap_t* _Nonnull self)
{
    const char* label = &self->argv[self->arg_idx++][1];

    for (;;) {
        const char ch = *label++;

        if (ch == '\0' || ch == '=') {
            break;
        }

        // Find the parameter for the label
        clap_param_t* param = clap_find_param_by_short_label(self, ch);
        if (param == NULL) {
            clap_error(self->argv[0], "unknown option '-%c'", ch);
            return EXIT_FAILURE;
        }

        // Update the parameter value
        const clap_status_t status = clap_update_named_param_value(self, param, 0, (*label == '=') ? label : NULL, false);
        if (status != 0) {
            return status;
        }
    }

    return EXIT_SUCCESS;
}


static void clap_activate_next_positional_param(clap_t* _Nonnull self)
{
    int i = self->cur_pos_param_idx + 1;

    while (i < self->cur_params.count) {
        const clap_param_t* p = &self->cur_params.p[i];

        switch (p->type) {
            case clap_type_string:
            case clap_type_enum:
            case clap_type_value:
            case clap_type_vararg:
                if (p->short_label == '\0' && (p->long_label == NULL || *p->long_label == '\0')) {
                    self->cur_pos_param_idx = i;
                    return;
            }
            break;

            default:
                break;
        }
        
        i++;
    }
    self->cur_pos_param_idx = i;
}

static clap_status_t clap_parse_command_param(clap_t* _Nonnull self, bool* _Nonnull pOutConsumed)
{
    const char* cmd_name = self->argv[self->arg_idx];
    clap_param_t* cmd_param = NULL;
    int cmd_idx;

    *pOutConsumed = false;

    for (size_t i = 0; i < self->cmd.entries_count; i++) {
        clap_param_t* cc = self->cmd.entries[i].decl;

        if (!strcmp(cmd_name, cc->u.cmd.name)) {
            cmd_param = cc;
            cmd_idx = i;
            break;
        }
    }

    if (cmd_param) {
        *(const char**)cmd_param->value = cmd_name;
        cmd_param->flags |= clap_flag_appeared;
        self->cmd.appeared = true;
        self->arg_idx++;

        const clap_status_t status = clap_enforce_required_params(self);
        if (status != 0) {
            return status;
        }

        clap_set_params(self, cmd_idx);
        *pOutConsumed = true;
        return EXIT_SUCCESS;
    }
    else if(self->cmd.required) {
        clap_error(self->argv[0], "unknown command '%s'", cmd_name);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

static clap_status_t clap_parse_positional_param(clap_t* _Nonnull self)
{
    bool didConsume = false;

    // Try to activate the first positional parameter if there is one
    if (self->cur_pos_param_idx == -1) {
        clap_activate_next_positional_param(self);
    }


    if (self->cur_pos_param_idx >= 0 && self->cur_pos_param_idx < self->cur_params.count) {
        clap_param_t* p = &self->cur_params.p[self->cur_pos_param_idx];

        if (p->type == clap_type_vararg) {
            self->should_interpret_args = false;
        }

        clap_update_positional_param_value(self, p);
        clap_activate_next_positional_param(self);
        didConsume = true;
    }
    else {
        // If we were not able to activate a positional parameter (in the global
        // parameter list), no command is currently selected, but we do have command
        // definitions then we'll interpret the provided parameter value as a command
        // selector
        if (self->cur_cmd_idx == -1 && self->cmd.entries_count > 0 && !self->cmd.appeared && self->should_interpret_args) {
            const clap_status_t status = clap_parse_command_param(self, &didConsume);
            if (status != 0) {
                return status;
            }
        }
    }

    if (!didConsume) {
        clap_error(self->argv[0], "superfluous parameter '%s'", self->argv[self->arg_idx]);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}


static clap_status_t clap_enforce_required_params(clap_t* _Nonnull self)
{
    char short_label_buffer[3];

    short_label_buffer[0] = '-';
    short_label_buffer[2] = '\0';

    for (int i = 0; i < self->cur_params.count; i++) {
        const clap_param_t* param = &self->cur_params.p[i];
        const uint8_t flags = param->flags;

        switch (param->type) {
            case clap_type_command:
                if (self->cmd.required && !self->cmd.appeared) {
                    clap_error(self->argv[0], "required command missing");
                    return EXIT_FAILURE;
                }
                break;

            case clap_type_boolean:
            case clap_type_integer:
            case clap_type_string:
            case clap_type_string_array:
            case clap_type_enum:
            case clap_type_vararg:
                if ((flags & clap_flag_required) == clap_flag_required && (flags & clap_flag_appeared) == 0) {
                    const char* label = NULL;
                    const char* label_prefix = NULL;

                    if (param->long_label && *param->long_label != '\0') {
                        label = param->long_label;
                        label_prefix = "--";
                    }
                    else if (param->short_label != '\0') {
                        short_label_buffer[1] = param->short_label;
                        label = short_label_buffer;
                    }

                    if (label) {
                        clap_error(self->argv[0], "required option '%s%s' missing", label_prefix, label);
                    }
                    else if (param->help && *param->help != '\0') {
                        clap_error(self->argv[0], param->help);
                    }
                    else {
                        clap_error(self->argv[0], "expected an argument");
                    }
                    return EXIT_FAILURE;
                }
                break;

            default:
                break;
        }
    }

    return EXIT_SUCCESS;
}

static clap_status_t clap_parse_args(clap_t* _Nonnull self)
{
    self->should_interpret_args = true;
    self->should_terminate = false;
    clap_status_t status = 0;

    while (self->arg_idx < self->argc && status == 0 && !self->should_terminate) {
        const char* ap = self->argv[self->arg_idx];

        if (self->should_interpret_args && *ap == '-' && *(ap + 1) != '\0') {
            ap++;

            if (*ap == '-' && *(ap + 1) == '\0') {
                // --
                self->arg_idx++;
                self->should_interpret_args = false;
            }
            else if (*ap == '-' && *(ap + 1) != '\0') {
                // --bla
                status = clap_parse_long_label_param(self);
            }
            else {
                // -bla
                status = clap_parse_short_label_param(self);
            }
        }
        else {
            // positional parameter
            status = clap_parse_positional_param(self);
        }
    }

    
    if (status == 0 && !self->should_terminate) {
        // Check that all required parameters have appeared in the command line
        status = clap_enforce_required_params(self);
    }

    if (status != 0 || self->should_terminate) {
        status |= 0x100;
    }

    return status;
}


int clap_parse(unsigned int options, clap_param_t* _Nonnull params, int argc, const char** argv)
{
    clap_t clap;

    clap_init(&clap, params, argc, argv);
    const clap_status_t status = clap_parse_args(&clap);
    clap_deinit(&clap);

    if (clap_should_exit(status)) {
        if ((options & clap_option_no_exit) == clap_option_no_exit) {
            return status;
        }
        else {
            exit(clap_exit_code(status));
            // NOT REACHED
        }
    }
    else {
        return EXIT_SUCCESS;
    }
}
