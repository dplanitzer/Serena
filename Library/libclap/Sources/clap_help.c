//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap_priv.h"
#include <stdio.h>
#include <string.h>
#ifdef __SERENA__
#include <System/Process.h>
#endif


// Prints the application name as it appears in argv[0]. This is just the name
// of the app without the platform specific path and extension.
void _clap_print_app_name(void)
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

static bool clap_print_usage(clap_t* _Nonnull self)
{
    const clap_param_t* p = self->params;
    int c = 0;

    while (p->type != clap_type_end && p->type != clap_type_command) {
        if (p->type == clap_type_usage && *p->u.text != '\0') {
            printf((c == 0) ? "usage: %s\n" : "       %s\n", p->u.text);
            c++;
        }
        p++;
    }

    if (c == 0) {
        printf("usage:\n");
    }

    return (c > 0) ? true : false;
}

static bool clap_print_prolog_epilog(clap_t* _Nonnull self, enum clap_type type, bool wantsLeadingNewline)
{
    const clap_param_t* p = self->params;
    int c = 0;

    while (p->type != clap_type_end && p->type != clap_type_command) {
        if (p->type == type && *p->u.text != '\0') {
            if (wantsLeadingNewline && c == 0) {
                putchar('\n');
            }
            puts(p->u.text);
            c++;
        }
        p++;
    }

    return (c > 0) ? true : false;
}

static bool clap_should_print_help_for_param(const clap_param_t* _Nonnull param)
{
    switch (param->type) {
        case clap_type_boolean:
        case clap_type_integer:
        case clap_type_string:
        case clap_type_string_array:
        case clap_type_enum:
        case clap_type_value:
        case clap_type_version:
        case clap_type_help:
            return true;

        default:
            return false;
    }
}

static void clap_print_param_help(const clap_param_t* _Nonnull p, int column_0_width)
{
    int cw = 0;

    if (p->short_label == '\0' && (p->long_label == NULL || *p->long_label == '\0') && (p->help == NULL || *p->help == '\0')) {
        return;
    }

    fputs("  ", stdout);
    cw += 2;

    if (p->short_label != '\0') {
        printf("-%c", p->short_label);
        cw += 2;
    }
    if (p->short_label != '\0' && p->long_label && *p->long_label != '\0') {
        fputs(", ", stdout);
        cw += 2;
    }
    if (p->long_label && *p->long_label != '\0') {
        printf("--%s", p->long_label);
        cw += 2 + strlen(p->long_label);
    }

    int nSpaces = (cw <= column_0_width) ? column_0_width - cw : 0;
    printf("%*s%s\n", 3 + nSpaces, "", p->help);
}

static void clap_print_params_help(clap_t* _Nonnull self)
{
    // Calculate the width of column #0. This is the column that contains the
    // parameter short & long names
    int column_0_width = 0;
    for (int i = 0; i < self->params_count; i++) {
        const clap_param_t* p = &self->params[i];
        int col_width = 2;

        if (p->type == clap_type_section && p->u.text && *p->u.text != '\0') {
            continue;
        }
        if (!clap_should_print_help_for_param(p)) {
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

        if (p->type == clap_type_section && p->u.text && *p->u.text != '\0') {
            if (i > 0) {
                putchar('\n');
            }
            puts(p->u.text);
            continue;
        }

        if (clap_should_print_help_for_param(p)) {
            clap_print_param_help(p, column_0_width);
        }
    }
}

static bool clap_print_commands_help(clap_t* _Nonnull self)
{
    if (self->cmds_count == 0) {
        return false;
    }

    puts("The following commands are supported:");

    int column_0_width = 0;
    for (int i = 0; i < self->cmds_count; i++) {
        const int w = (int)strlen(self->cmds[i]->u.cmd.name);

        if (w > column_0_width) {
            column_0_width = w;
        }
    }

    for (int i = 0; i < self->cmds_count; i++) {
        const clap_param_t* cp = self->cmds[i];
        const int cw = strlen(cp->u.cmd.name);
        const int nSpaces = (cw <= column_0_width) ? column_0_width - cw : 0;

        printf("  %s%*s", cp->u.cmd.name, 2 + nSpaces, "");
        if (cp->u.cmd.usage && *cp->u.cmd.usage != '\0') {
            printf(" %s", cp->u.cmd.usage);
        }
        if (cp->help && *cp->help != '\0') {
            printf("   %s", cp->help);
        }
        putchar('\n');
    }

    return true;
}

void _clap_help(clap_t* _Nonnull self, const clap_param_t* _Nonnull param)
{
    const hasUsage = clap_print_usage(self);
    const hasProlog = clap_print_prolog_epilog(self, clap_type_prolog, hasUsage);

    if (hasProlog || hasUsage) {
        putchar('\n');
    }

    const bool hasCommands = clap_print_commands_help(self);

    if (hasCommands) {
        putchar('\n');
    }

    clap_print_params_help(self);

    clap_print_prolog_epilog(self, clap_type_epilog, true);

    exit(EXIT_SUCCESS);
}
