//
//  locale.c
//  libc
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <locale.h>
#include <limits.h>
#include <string.h>

static struct lconv    _CurrentLocale;

void __locale_init(void)
{
    setlocale(LC_ALL, "C");
}

// XXX Temp solution
char* _Nonnull setlocale(int category, const char* _Nullable locale)
{
    if (category < LC_ALL || category > _LC_LAST) {
        return NULL;
    }

    if (locale == NULL) {
        return (char*)locale;
    }

    if (*locale == '\0' || !strcmp(locale, "C")) {
        if (category == LC_ALL || category == LC_NUMERIC) {
            _CurrentLocale.decimal_point = ".";
            _CurrentLocale.thousands_sep = "";
            _CurrentLocale.grouping = "";
        }
        if (category == LC_ALL || category == LC_MONETARY) {
            _CurrentLocale.mon_decimal_point = "";
            _CurrentLocale.mon_thousands_sep = "";
            _CurrentLocale.mon_grouping = "";
            _CurrentLocale.positive_sign = "";
            _CurrentLocale.negative_sign = "";
            _CurrentLocale.currency_symbol = "";
            _CurrentLocale.frac_digits = CHAR_MAX;
            _CurrentLocale.p_cs_precedes = CHAR_MAX;
            _CurrentLocale.n_cs_precedes = CHAR_MAX;
            _CurrentLocale.p_sep_by_space = CHAR_MAX;
            _CurrentLocale.n_sep_by_space = CHAR_MAX;
            _CurrentLocale.p_sign_posn = CHAR_MAX;
            _CurrentLocale.n_sign_posn = CHAR_MAX;
            _CurrentLocale.int_curr_symbol = "";
            _CurrentLocale.int_frac_digits = CHAR_MAX;
            _CurrentLocale.int_p_cs_precedes = CHAR_MAX;
            _CurrentLocale.int_n_cs_precedes = CHAR_MAX;
            _CurrentLocale.int_p_sep_by_space = CHAR_MAX;
            _CurrentLocale.int_n_sep_by_space = CHAR_MAX;
            _CurrentLocale.int_p_sign_posn = CHAR_MAX;
            _CurrentLocale.int_n_sign_posn = CHAR_MAX;
        }
    }
    else {
        return NULL;
    }

    return (char*)locale;
}

struct lconv * _Nonnull localeconv(void)
{
    return &_CurrentLocale;
}
