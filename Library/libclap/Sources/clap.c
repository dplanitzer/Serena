//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap_priv.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


static void clap_set_params(clap_t* _Nonnull self, clap_param_t* _Nullable params, bool isCommand);
static clap_status_t clap_enforce_required_params(clap_t* _Nonnull self);


static void clap_init(clap_t* _Nonnull self, clap_param_t* _Nullable params, int argc, const char** argv)
{
    memset(self, 0, sizeof(clap_t));
    self->arg_idx = 1;
    self->argc = argc;
    self->argv = argv;
    self->should_interpret_args = true;

    clap_set_params(self, params, false);
}

static void clap_deinit(clap_t* _Nullable self)
{
    if (self) {
        self->params = NULL;
        self->params_count = 0;

        self->vararg_param = NULL;

        free(self->cmds);
        self->cmds = NULL;
        self->cmds_count = 0;

        self->argc = 0;
        self->argv = NULL;
    }
}


static void clap_set_params(clap_t* _Nonnull self, clap_param_t* _Nullable params, bool isCommand)
{
    clap_param_t* p = params;
    size_t cmds_count = 0;

    self->params = params;
    self->params_count = 0;

    self->vararg_param = NULL;

    free(self->cmds);
    self->cmds = NULL;
    self->cmds_count = 0;
    self->cmd_required = false;
    self->cmd_appeared = false;

    // Analyze the parameter list. If this is a top-level parameter list (isCommand == false)
    // then iterate until we hit the clap_type_end parameter. If however this is a command
    // parameter list, then stop iterating either when we hit the end of the parameter list
    // or we hit the next command declaration.
    while (p) {
        if (p->type == clap_type_end || (isCommand && p->type == clap_type_command)) {
            break;
        }

        p->flags &= ~clap_flag_appeared;

        switch (p->type) {
            case clap_type_command:
                if ((p->flags & clap_flag_required) == clap_flag_required) {
                    self->cmd_required = true;
                }
                cmds_count++;
                break;

            case clap_type_vararg:
                if (self->vararg_param == NULL && ((!isCommand && cmds_count == 0) || isCommand)) {
                    self->vararg_param = p;
                }
                break;

            default:
                break;
        }

        p++;
        self->params_count++;
    }


    // Collect the command declarations
    if (cmds_count > 0) {
        self->cmds = malloc(sizeof(clap_param_t*) * cmds_count);

        p = params;
        while (self->cmds_count < cmds_count) {
            if (p->type == clap_type_command) {
                self->cmds[self->cmds_count++] = p;
                if (self->cmd_required) {
                    p->flags |= clap_flag_required;
                }
            }

            p++;
        }
    }
}


static void clap_version(clap_t* _Nonnull self, const clap_param_t* _Nonnull param)
{
    if (param->u.text && *param->u.text != '\0') {
        puts(param->u.text);
    }
}


static clap_param_t* _Nullable clap_find_param_by_long_label(clap_t* _Nonnull self, const char* _Nonnull label, size_t label_len)
{
    for (int i = 0; i < self->params_count; i++) {
        clap_param_t* p = &self->params[i];

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
    for (int i = 0; i < self->params_count; i++) {
        clap_param_t* p = &self->params[i];

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


static clap_status_t clap_parse_command_param(clap_t* _Nonnull self, bool* _Nonnull pOutConsumed)
{
    const char* cmd_name = self->argv[self->arg_idx];
    clap_param_t* cmd_param = NULL;

    *pOutConsumed = false;

    for (size_t i = 0; i < self->cmds_count; i++) {
        clap_param_t* cc = self->cmds[i];

        if (!strcmp(cmd_name, cc->u.cmd.name)) {
            cmd_param = cc;
            break;
        }
    }

    if (cmd_param) {
        *(const char**)cmd_param->value = cmd_name;
        cmd_param->flags |= clap_flag_appeared;
        self->cmd_appeared = true;
        self->arg_idx++;

        const clap_status_t status = clap_enforce_required_params(self);
        if (status != 0) {
            return status;
        }

        clap_set_params(self, cmd_param + 1, true);
        *pOutConsumed = true;
        return EXIT_SUCCESS;
    }
    else if(self->cmd_required) {
        clap_error(self->argv[0], "unknown command '%s'", cmd_name);
        return EXIT_FAILURE;
    }
    else {
        return EXIT_SUCCESS;
    }
}

static clap_status_t clap_parse_positional_param(clap_t* _Nonnull self)
{
    clap_param_t* param;
    bool didConsume = false;

    if (self->cmds_count > 0 && !self->cmd_appeared && self->should_interpret_args) {
        const clap_status_t status = clap_parse_command_param(self, &didConsume);
        if (status != 0) {
            return status;
        }
    }

    if (!didConsume && self->vararg_param) {
        self->should_interpret_args = false;

        const clap_status_t status = clap_update_string_array_param_value(self, self->vararg_param, 0, NULL, false);
        if (status != 0) {
            return status;
        }

        self->vararg_param->flags |= clap_flag_appeared;
        didConsume = true;
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

    for (size_t i = 0; i < self->params_count; i++) {
        const clap_param_t* param = &self->params[i];
        const uint8_t flags = param->flags;

        switch (param->type) {
            case clap_type_command:
                if (self->cmd_required && !self->cmd_appeared) {
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
