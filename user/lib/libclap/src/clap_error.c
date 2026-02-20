//
//  clap.c
//  libclap
//
//  Created by Dietmar Planitzer on 5/27/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "clap_priv.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>


// Prints the application name as it appears in argv[0]. This is just the name
// of the app without the platform specific path and extension.
static void _clap_print_app_name(const char* _Nonnull proc_name)
{
    const char* app_name = "";
    int app_name_len = 0;

#if __SERENA__
    if (*proc_name != '\0') {
        const char* sp = strrchr(proc_name, '/');
        
        app_name = (sp) ? sp + 1 : proc_name;
        app_name_len = (int) strlen(app_name);
    }
#elif _WIN32
    if (*proc_name != '\0') {
        const char* sp = strrchr(proc_name, '\\');
        const char* ep = strrchr(proc_name, '.');

        if (sp && ep) {
            app_name = sp + 1;
            app_name_len = ep - 1 - sp;
        }
        else if (sp == NULL && ep) {
            app_name = proc_name;
            app_name_len = ep - 1 - app_name;
        }
        else {
            app_name = proc_name;
            app_name_len = (int) strlen(app_name);
        }
    }
#else
    // XXX
    if (*proc_name) {
        app_name = proc_name;
        app_name_len = strlen(proc_name);
    }
#endif

    if (app_name_len > 0) {
        fwrite(app_name, app_name_len, 1, stderr);
        fwrite(": ", 2, 1, stderr);
    }
}

void clap_verror(const char* _Nonnull proc_name, const char* format, va_list ap)
{
    _clap_print_app_name(proc_name);
    vfiprintf(stderr, format, ap);
    fputc('\n', stderr);
}

void clap_error(const char* _Nonnull proc_name, const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_verror(proc_name, format, ap);
    va_end(ap);
}

void clap_vparam_error(const char* _Nonnull proc_name, const clap_param_t* _Nonnull param, unsigned int eo, const char* format, va_list ap)
{
    const char* param_kind = (param->type == clap_type_boolean) ? "switch" : "option";

    _clap_print_app_name(proc_name);

    if (param->short_label != '\0' || (param->long_label && *param->long_label != '\0')) {
        if ((eo & clap_eo_long_label) == clap_eo_long_label) {
            fprintf(stderr, "%s '--%s': ", param_kind, param->long_label);
        }
        else {
            fprintf(stderr, "%s '-%c': ", param_kind, param->short_label);
        }
    }

    vfiprintf(stderr, format, ap);
    fputc('\n', stderr);
}

void clap_param_error(const char* _Nonnull proc_name, const clap_param_t* _Nonnull param, unsigned int eo, const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_vparam_error(proc_name, param, eo, format, ap);
    va_end(ap);
}
