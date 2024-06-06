//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap_priv.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __SERENA__
#include <System/Process.h>
#endif


static const char* const clap_g_type_string[] = {
    "boolean",
    "integer",
    "string",
    "strings",
    "enumeration",
    "command",
    "value",

    "vararg",
    "version",
    "help",
    "usage",
    "prolog",
    "help section",
    "epilog",
    "end"
};

static void clap_set_params(clap_t* _Nonnull self, clap_param_t* _Nullable params, bool isCommand);
static void clap_enforce_required_params(clap_t* _Nonnull self);


static void clap_init(clap_t* _Nonnull self, clap_param_t* _Nullable params, int argc, const char** argv)
{
    memset(self, 0, sizeof(clap_t));
    self->arg_idx = 1;
    self->argc = argc;
    self->argv = argv;
    self->should_interpret_args = true;
    self->short_label_buffer[0] = '-';
    self->short_label_buffer[2] = '\0';

    clap_set_params(self, params, false);
}

static void clap_deinit(clap_t* _Nullable self)
{
    if (self) {
        self->params = NULL;
        self->params_count = 0;

        self->vararg_param = NULL;

        self->cur_label = NULL;
        self->cur_label_len = 0;

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
    int cmds_count = 0;

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


static void clap_verror(const char* format, va_list ap)
{
    _clap_print_app_name();
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void clap_error(const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_verror(format, ap);
    va_end(ap);
}

static void clap_vparam_error(clap_t* _Nonnull self, const clap_param_t* _Nonnull param, const char* format, va_list ap)
{
    const char* param_kind = (param->type == clap_type_boolean) ? "switch" : "option";

    _clap_print_app_name();

    if (self->cur_label && self->cur_label_len > 0) {
        fprintf(stderr, "%s '%.*s': ", param_kind, self->cur_label_len, self->cur_label);
    }

    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void clap_param_error(struct clap_t* _Nonnull self, const clap_param_t* _Nonnull param, const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_vparam_error(self, param, format, ap);
    va_end(ap);
}


static void clap_version(clap_t* _Nonnull self, const clap_param_t* _Nonnull param)
{
    if (param->u.text && *param->u.text != '\0') {
        puts(param->u.text);
    }

    exit(EXIT_SUCCESS);
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


static void clap_update_bool_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, bool isUnset)
{
    if (eq) {
        clap_param_error(self, param, "unexpected value '%s'", eq + 1);
        // not reached
    }

    *(bool*)param->value = (isUnset) ? false : true;
}

static void clap_update_int_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    const char* vptr;

    if (eq) {
        vptr = eq + 1;
    }
    else if (self->arg_idx < self->argc) {
        vptr = self->argv[self->arg_idx++];
    }
    else {
        clap_param_error(self, param, "expected an integer");
        // not reached
    }


    errno = 0;
    *(int*)param->value = (int)strtol(vptr, NULL, 10);

    if (errno == ERANGE) {
        clap_param_error(self, param, "integer %s is out of range", vptr);
    }
}

static void clap_update_string_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    const char* vptr;

    if (eq) {
        vptr = eq + 1;
    }
    else if (self->arg_idx < self->argc) {
        vptr = self->argv[self->arg_idx++];
    }
    else {
        clap_param_error(self, param, "expected a string");
        // not reached
    }


    *(const char**)param->value = vptr;
}

static void clap_update_string_array_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, bool shouldEndAtLabel)
{
    const char* vptr;

    if (eq) {
        clap_param_error(self, param, "expects separate strings");
        // not reached
    }
    if (self->arg_idx >= self->argc) {
        clap_param_error(self, param, "expected at least one string");
        // not reached
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
        return;
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
        for (int i = startIdx; i < dashDashIdx; i++) {
            array->strings[j++] = self->argv[i];
        }
        for (int i = dashDashIdx + 1; i < endIdx; i++) {
            array->strings[j++] = self->argv[i];
        }
    }
}

static void clap_update_enum_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    const char** enum_strs = param->u.enum_strings;
    const char* user_str = "";
    int* iptr = (int*)param->value;
    int i = 0;

    param->value = &user_str;
    clap_update_string_param_value(self, param, eq);

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
    }
    else {
        clap_param_error(self, param, "unknown enum value '%s'", user_str);
        // not reached
    }
}

static void clap_update_value_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    const char* user_str = "";
    void* vptr = param->value;

    param->value = &user_str;
    clap_update_string_param_value(self, param, eq);
    param->value = vptr;

    param->u.value_func(self, param, user_str);
}

static void clap_update_named_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, bool isUnset)
{
    switch (param->type) {
        case clap_type_boolean:
            // bool (no value)
            clap_update_bool_param_value(self, param, eq, isUnset);
            break;

        case clap_type_integer:
            // int (1 value)
            clap_update_int_param_value(self, param, eq);
            break;

        case clap_type_string:
            // const char* (1 value)
            clap_update_string_param_value(self, param, eq);
            break;

        case clap_type_string_array:
            // [const char*] (multiple values)
            clap_update_string_array_param_value(self, param, eq, true);
            break;

        case clap_type_enum:
            // int (1 value)
            clap_update_enum_param_value(self, param, eq);
            break;

        case clap_type_value:
            // const char* (1 value)
            clap_update_value_param_value(self, param, eq);
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
}


// Parses a long label option like:
// --foo
// --no-foo     (synthesized)
// --foo=x
static void clap_parse_long_label_param(clap_t* _Nonnull self)
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

    self->cur_label = label;
    self->cur_label_len = (eq) ? eq - label : strlen(label);

    // Find the parameter for the label
    clap_param_t* param = clap_find_param_by_long_label(self, label_to_match, label_to_match_len);
    if (param == NULL || (param && isUnset && param->type != clap_type_boolean)) {
        clap_error("unknown option '%.*s'", self->cur_label_len, self->cur_label);
        // not reached
    }

    // Update the parameter value
    clap_update_named_param_value(self, param, eq, isUnset);

    self->cur_label = NULL;
    self->cur_label_len = 0;
}

// Parses a short label option like:
// -b
// -b=x
// -bcdef
// -bcdef=x
static void clap_parse_short_label_param(clap_t* _Nonnull self)
{
    const char* label = &self->argv[self->arg_idx++][1];

    self->cur_label = self->short_label_buffer;
    self->cur_label_len = 2;

    for (;;) {
        const char ch = *label++;

        if (ch == '\0' || ch == '=') {
            break;
        }
        self->short_label_buffer[1] = ch;

        // Find the parameter for the label
        clap_param_t* param = clap_find_param_by_short_label(self, ch);
        if (param == NULL) {
            clap_error("unknown option '%.*s'", self->cur_label_len, self->cur_label);
            // not reached
        }

        // Update the parameter value
        clap_update_named_param_value(self, param, (*label == '=') ? label : NULL, false);
    }

    self->cur_label = NULL;
    self->cur_label_len = 0;
}


static bool clap_parse_command_param(clap_t* _Nonnull self)
{
    const char* cmd_name = self->argv[self->arg_idx];
    clap_param_t* cmd_param = NULL;

    for (int i = 0; i < self->cmds_count; i++) {
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

        clap_enforce_required_params(self);
        clap_set_params(self, cmd_param + 1, true);
        return true;
    }
    else if(self->cmd_required) {
        clap_error("unknown command '%s'", cmd_name);
        // not reached
        return false;
    }
    else {
        return false;
    }
}

static void clap_parse_positional_param(clap_t* _Nonnull self)
{
    clap_param_t* param;
    bool didConsume = false;

    if (self->cmds_count > 0 && !self->cmd_appeared && self->should_interpret_args) {
        didConsume = clap_parse_command_param(self);
    }

    if (!didConsume && self->vararg_param) {
        self->should_interpret_args = false;
        self->cur_label = NULL;
        self->cur_label_len = 0;

        clap_update_string_array_param_value(self, self->vararg_param, NULL, false);
        self->vararg_param->flags |= clap_flag_appeared;
        didConsume = true;
    }

    if (!didConsume) {
        clap_error("superfluous parameter '%s'", self->argv[self->arg_idx]);
        // not reached
    }
}


static void clap_enforce_required_params(clap_t* _Nonnull self)
{
    for (int i = 0; i < self->params_count; i++) {
        const clap_param_t* param = &self->params[i];
        const uint8_t flags = param->flags;

        switch (param->type) {
            case clap_type_command:
                if (self->cmd_required && !self->cmd_appeared) {
                    clap_error("required command missing");
                }
                break;

            case clap_type_boolean:
            case clap_type_integer:
            case clap_type_string:
            case clap_type_string_array:
            case clap_type_enum:
                if ((flags & clap_flag_required) == clap_flag_required && (flags & clap_flag_appeared) == 0) {
                    const char* label = NULL;
                    const char* label_prefix = NULL;

                    if (param->long_label && *param->long_label != '\0') {
                        label = param->long_label;
                        label_prefix = "--";
                    }
                    else if (param->short_label != '\0') {
                        self->short_label_buffer[1] = param->short_label;
                        label = self->short_label_buffer;
                    }

                    if (label) {
                        clap_error("required option '%s%s' missing", label_prefix, label);
                    }
                    else {
                        clap_error("expected a %s", clap_g_type_string[param->type]);
                    }
                    // not reached
                }
                break;

            default:
                break;
        }
    }
}

static void clap_parse_args(clap_t* _Nonnull self)
{
    self->should_interpret_args = true;

    while (self->arg_idx < self->argc) {
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
                clap_parse_long_label_param(self);
            }
            else {
                // -bla
                clap_parse_short_label_param(self);
            }
        }
        else {
            // positional parameter
            clap_parse_positional_param(self);
        }
    }

    
    // Check that all required parameters have appeared in the command line
    clap_enforce_required_params(self);
}


void clap_parse(clap_param_t* _Nonnull params, int argc, const char** argv)
{
    clap_t clap;

    clap_init(&clap, params, argc, argv);
    clap_parse_args(&clap);
    clap_deinit(&clap);
}
