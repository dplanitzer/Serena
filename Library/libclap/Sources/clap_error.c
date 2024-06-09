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
#ifdef __SERENA__
#include <System/Process.h>
#endif


// Prints the application name as it appears in argv[0]. This is just the name
// of the app without the platform specific path and extension.
static void _clap_print_app_name(void)
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

void clap_verror(const char* format, va_list ap)
{
    _clap_print_app_name();
    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
}

void clap_error(const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_verror(format, ap);
    va_end(ap);
}

void clap_vparam_error(const clap_param_t* _Nonnull param, unsigned int eo, const char* format, va_list ap)
{
    const char* param_kind = (param->type == clap_type_boolean) ? "switch" : "option";

    _clap_print_app_name();

    if ((eo & clap_eo_long_label) == clap_eo_long_label) {
        fprintf(stderr, "%s '--%s': ", param_kind, param->long_label);
    }
    else {
        fprintf(stderr, "%s '-%c': ", param_kind, param->short_label);
    }

    vfprintf(stderr, format, ap);
    fputc('\n', stderr);
}

void clap_param_error(const clap_param_t* _Nonnull param, unsigned int eo, const char* format, ...)
{
    va_list ap;
    
    va_start(ap, format);
    clap_vparam_error(param, eo, format, ap);
    va_end(ap);
}
