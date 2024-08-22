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


static const struct lconv   _C_locale = {
    .decimal_point = ".",
    .thousands_sep = "",
    .grouping = "",
    .mon_decimal_point = "",
    .mon_thousands_sep = "",
    .mon_grouping = "",
    .positive_sign = "",
    .negative_sign = "",
    .currency_symbol = "",
    .frac_digits = CHAR_MAX,
    .p_cs_precedes = CHAR_MAX,
    .n_cs_precedes = CHAR_MAX,
    .p_sep_by_space = CHAR_MAX,
    .n_sep_by_space = CHAR_MAX,
    .p_sign_posn = CHAR_MAX,
    .n_sign_posn = CHAR_MAX,
    .int_curr_symbol = "",
    .int_frac_digits = CHAR_MAX,
    .int_p_cs_precedes = CHAR_MAX,
    .int_n_cs_precedes = CHAR_MAX,
    .int_p_sep_by_space = CHAR_MAX,
    .int_n_sep_by_space = CHAR_MAX,
    .int_p_sign_posn = CHAR_MAX,
    .int_n_sign_posn = CHAR_MAX
};

static struct lconv    _CurrentLocale;


void __locale_init(void)
{
    setlocale(LC_ALL, "C");
}


static const struct lconv* _Nullable __get_locale(const char* _Nonnull locale)
{
    if (*locale == '\0' || !strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
        return &_C_locale;
    }
    else {
        return NULL;
    }
}


static void __copy_collate_category(struct lconv* _Nonnull _Restrict dl, const struct lconv* _Nonnull _Restrict sl)
{
}

static void __copy_ctype_category(struct lconv* _Nonnull _Restrict dl, const struct lconv* _Nonnull _Restrict sl)
{
}

static void __copy_monetary_category(struct lconv* _Nonnull _Restrict dl, const struct lconv* _Nonnull _Restrict sl)
{
    dl->mon_decimal_point = sl->mon_decimal_point;
    dl->mon_thousands_sep = sl->mon_thousands_sep;
    dl->mon_grouping = sl->mon_grouping;
    dl->positive_sign = sl->positive_sign;
    dl->negative_sign = sl->negative_sign;
    dl->currency_symbol = sl->currency_symbol;
    dl->frac_digits = sl->frac_digits;
    dl->p_cs_precedes = sl->p_cs_precedes;
    dl->n_cs_precedes = sl->n_cs_precedes;
    dl->p_sep_by_space = sl->p_sep_by_space;
    dl->n_sep_by_space = sl->n_sep_by_space;
    dl->p_sign_posn = sl->p_sign_posn;
    dl->n_sign_posn = sl->n_sign_posn;
    dl->int_curr_symbol = sl->int_curr_symbol;
    dl->int_frac_digits = sl->int_frac_digits;
    dl->int_p_cs_precedes = sl->int_p_cs_precedes;
    dl->int_n_cs_precedes = sl->int_n_cs_precedes;
    dl->int_p_sep_by_space = sl->int_p_sep_by_space;
    dl->int_n_sep_by_space = sl->int_n_sep_by_space;
    dl->int_p_sign_posn = sl->int_p_sign_posn;
    dl->int_n_sign_posn = sl->int_n_sign_posn;
}

static void __copy_numeric_category(struct lconv* _Nonnull _Restrict dl, const struct lconv* _Nonnull _Restrict sl)
{
    dl->decimal_point = sl->decimal_point;
    dl->thousands_sep = sl->thousands_sep;
    dl->grouping = sl->grouping;
}

static void __copy_time_category(struct lconv* _Nonnull _Restrict dl, const struct lconv* _Nonnull _Restrict sl)
{
}


char* _Nonnull setlocale(int category, const char* _Nullable locale)
{
    if (locale == NULL) {
        return (char*)locale;
    }

    const struct lconv* sl = __get_locale(locale);
    struct lconv* dl = &_CurrentLocale;

    if (sl == NULL) {
        return NULL;
    }

    switch (category) {
        case LC_ALL:
            __copy_collate_category(dl, sl);
            __copy_ctype_category(dl, sl);
            __copy_monetary_category(dl, sl);
            __copy_numeric_category(dl, sl);
            __copy_time_category(dl, sl);
            break;

        case LC_COLLATE:
            __copy_collate_category(dl, sl);
            break;

        case LC_CTYPE:
            __copy_ctype_category(dl, sl);
            break;

        case LC_MONETARY:
            __copy_monetary_category(dl, sl);
            break;

        case LC_NUMERIC:
            __copy_numeric_category(dl, sl);
            break;

        case LC_TIME:
            __copy_time_category(dl, sl);
            break;

        default:
            return NULL;
    }

    return (char*)locale;
}

struct lconv * _Nonnull localeconv(void)
{
    return &_CurrentLocale;
}
