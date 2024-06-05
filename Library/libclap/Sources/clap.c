//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef __SERENA__
#include <System/Process.h>
#endif


typedef struct clap_t {
    clap_param_t* _Nonnull  params;
    int                     params_count;
    clap_param_t* _Nullable cmd_param;      // First command type parameter in the parameter list; NULL if none exists
    clap_param_t* _Nullable vararg_param;   // First vararg type parameter in the parameter list; NULL if none exists

    const char**            argv;
    int                     argc;
    int                     arg_idx;

    bool                    should_interpret_args;  // If true then args are interpreted; if false then they are always assigned to the varargs

    const char* _Nullable   cur_label;
    int                     cur_label_len;

    char                    short_label_buffer[3];
} clap_t;


static const char* const clap_g_type_string[] = {
    "boolean",
    "integer",
    "string",
    "strings",
    "enumeration",
    "command",
    "value",

    "vararg",
    "help",
    "help section",
    "end"
};

static void clap_set_params(clap_t* _Nonnull self, clap_param_t* _Nullable params);


static void clap_init(clap_t* _Nonnull self, clap_param_t* _Nullable params, int argc, const char** argv)
{
    memset(self, 0, sizeof(clap_t));
    self->arg_idx = 1;
    self->argc = argc;
    self->argv = argv;
    self->should_interpret_args = true;
    self->short_label_buffer[0] = '-';
    self->short_label_buffer[2] = '\0';

    clap_set_params(self, params);
}

static void clap_deinit(clap_t* _Nullable self)
{
    if (self) {
        self->params = NULL;
        self->params_count = 0;

        self->cmd_param = NULL;
        self->vararg_param = NULL;

        self->cur_label = NULL;
        self->cur_label_len = 0;

        self->argc = 0;
        self->argv = NULL;
    }
}


static void clap_set_params(clap_t* _Nonnull self, clap_param_t* _Nullable params)
{
    assert(params != NULL);
    bool hasEnd = false;

    self->params = params;
    self->params_count = 0;

    self->cmd_param = NULL;
    self->vararg_param = NULL;

    for (int i = 0; i < 100; i++) {
        clap_param_t* p = &params[i];

        p->flags &= ~clap_flag_appeared;

        if (p->type == clap_type_end) {
            p->flags |= clap_flag_appeared;
            hasEnd = true;
            break;
        }

        if (p->type == clap_type_command && self->cmd_param == NULL) {
            self->cmd_param = p;
        }
        if (p->type == clap_type_vararg && self->vararg_param == NULL) {
            self->vararg_param = p;
        }

        self->params_count++;
    }
    assert(hasEnd);
}


// Prints the application name as it appears in argv[0]. This is just the name
// of the app without the platform specific path and extension.
static void clap_print_app_name(void)
{
    const char* app_name = "";
    int app_name_len = 0;

#if __SERENA__
    ProcessArguments* pa = Process_GetArguments();

    if (pa->argc > 0 && pa->argv[0] && pa->argv[0][0] != '\0') {
        const char* sp = strrchr(pa->argv[0], '/');
        
        app_name = (sp) ? sp + 1 : pa->argv[0];
        app_name_len = (int) strlen(app_name);
    }
#elif _WIN32
    char* argv_zero = NULL;

    _get_pgmptr(&argv_zero);
    if (argv_zero && *argv_zero != '\0') {
        const char* sp = strrchr(argv_zero, '\\');
        const char* ep = strrchr(argv_zero, '.');

        if (sp && ep) {
            app_name = sp + 1;
            app_name_len = ep - 1 - sp;
        }
        else if (sp == NULL && ep) {
            app_name = argv_zero;
            app_name_len = ep - 1 - app_name;
        }
        else {
            app_name = argv_zero;
            app_name_len = (int) strlen(app_name);
        }
    }
#else
    // XXX
#endif

    if (app_name_len > 0) {
        fprintf(stderr, "%.*s: ", app_name_len, app_name);
    }
}

static void clap_verror(const char* format, va_list ap)
{
    clap_print_app_name();
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

    clap_print_app_name();

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


static void clap_print_app_usages(const clap_help_t* _Nonnull help)
{
    const char** usages = help->usages;

    // Usage line(s)
    if (usages && *usages) {
        fprintf(stdout, "usage: %s\n", *usages++);

        while (*usages) {
            fprintf(stdout, "       %s\n", *usages++);
        }
    }
    else {
        fprintf(stdout, "usage:\n");
    }
}

static void clap_print_param_help(const clap_param_t* _Nonnull p, int column_0_width)
{
    int cw = 0;

    if (p->short_label == '\0' && (p->long_label == NULL || *p->long_label == '\0') && (p->help == NULL || *p->help == '\0')) {
        return;
    }

    fprintf(stdout, "  ");
    cw += 2;

    if (p->short_label != '\0') {
        fprintf(stdout, "-%c", p->short_label);
        cw += 2;
    }
    if (p->short_label != '\0' && p->long_label && *p->long_label != '\0') {
        fprintf(stdout, ", ");
        cw += 2;
    }
    if (p->long_label && *p->long_label != '\0') {
        fprintf(stdout, "--%s", p->long_label);
        cw += 2 + strlen(p->long_label);
    }

    int nspaces = (cw <= column_0_width) ? column_0_width - cw : 0;
    fprintf(stdout, "%*s%s\n", 3 + nspaces, "", p->help);
}

static void clap_print_help(clap_t* _Nonnull self, const clap_help_t* _Nonnull help)
{
    clap_print_app_usages(help);


    // Prolog aka description or short documentation
    if (help->prolog && *help->prolog != '\0') {
        fprintf(stdout, "%s\n", help->prolog);
    }

    fputc('\n', stdout);


    // Print the commands, if they exist
    if (self->cmd_param) {
        const clap_command_set_t* cmd_set = self->cmd_param->u.cmd_set;

        fprintf(stdout, "The following commands are supported:\n");

        const char** pname = &cmd_set->names[0];
        const char** pusage = &cmd_set->usages[0];
        const char** phelp = &cmd_set->helps[0];

        while(*pname) {
            fprintf(stdout, "  %s", *pname);
            if (*pusage) {
                fprintf(stdout, " %s", *pusage);
            }
            if (*phelp) {
                fprintf(stdout, "   %s", *phelp);
            }
            fputc('\n', stdout);

            pname++;
            pusage++;
            phelp++;
        }
    }

    fputc('\n', stdout);


    // Calculate the width of column #0. This is the column that contains the
    // parameter short & long names
    int column_0_width = 0;
    for (int i = 0; i < self->params_count; i++) {
        const clap_param_t* p = &self->params[i];
        int col_width = 2;

        if (p->type == clap_type_section && p->u.title && *p->u.title != '\0') {
            continue;
        }

        if (p->short_label != '\0') {
            col_width += 2;
        }
        if (p->short_label != '\0' && p->long_label && *p->long_label != '\0') {
            col_width += 2;
        }
        if (p->long_label && *p->long_label != '\0') {
            col_width += 2 + strlen(p->long_label);
        }

        if (col_width > column_0_width) {
            column_0_width = col_width;
        }
    }


    // Print the parameter descriptions
    for (int i = 0; i < self->params_count; i++) {
        const clap_param_t* p = &self->params[i];

        if (p->type == clap_type_section && p->u.title && *p->u.title != '\0') {
            if (i > 0) {
                fputc('\n', stdout);
            }
            fprintf(stdout, "%s\n", p->u.title);
            continue;
        }

        clap_print_param_help(p, column_0_width);
    }


    // Print the epilog aka trailing documentation
    if (help->epilog && *help->epilog != '\0') {
        fprintf(stdout, "\n%s\n", help->epilog);
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

static void _clap_update_enum_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, const char** enum_strs, const char* _Nonnull msg)
{
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
        clap_param_error(self, param, "unknown %s '%s'", msg, user_str);
        // not reached
    }
}

static void clap_update_enum_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    _clap_update_enum_param_value(self, param, eq, param->u.enum_strings, "unknown enum value");
}

static void clap_update_command_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq)
{
    _clap_update_enum_param_value(self, param, eq, param->u.cmd_set->names, "command");
    clap_set_params(self, param->u.cmd_set->params[*(int*)param->value]);
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

static void clap_update_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, bool isUnset)
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

        case clap_type_command:
            // int (1 value)
            clap_update_command_param_value(self, param, eq);
            break;

        case clap_type_value:
            // const char* (1 value)
            clap_update_value_param_value(self, param, eq);
            break;

        case clap_type_vararg:
            // [const char*] (multiple values)
            clap_update_string_array_param_value(self, param, eq, false);
            break;

        case clap_type_help:
            clap_print_help(self, &param->u.help);
            break;

        default:
            break;
    }

    param->flags |= clap_flag_appeared;
}

static void clap_update_named_param_value(clap_t* _Nonnull self, clap_param_t* _Nonnull param, const char* _Nullable eq, bool isUnset)
{
    clap_update_param_value(self, param, eq, isUnset);
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


static void clap_parse_positional_param(clap_t* _Nonnull self)
{
    clap_param_t* param;

    if (self->cmd_param && (self->cmd_param->flags & clap_flag_appeared) == 0 && self->should_interpret_args) {
        param = self->cmd_param;
    }
    else if (self->vararg_param) {
        param = self->vararg_param;
        self->should_interpret_args = false;
    }
    else {
        param = NULL;
    }


    if (param) {
        self->cur_label = NULL;
        self->cur_label_len = 0;
        clap_update_param_value(self, param, NULL, false);
    }
    else {
        clap_error("superfluous parameter '%s'", self->argv[self->arg_idx]);
        // not reached
    }
}


static void clap_enforce_required_params(clap_t* _Nonnull self)
{
    for (int i = 0; i < self->params_count; i++) {
        const clap_param_t* param = &self->params[i];
        const uint8_t flags = param->flags;

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
