//
//  locale_vars.c
//  libc
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "_locale.h"
#include <limits.h>


static locale_t __LOCALE_C = {
    {NULL},
    {
        .decimal_point      = ".",
        .thousands_sep      = "",
        .grouping           = "",
        .mon_decimal_point  = "",
        .mon_thousands_sep  = "",
        .mon_grouping       = "",
        .positive_sign      = "",
        .negative_sign      = "",
        .currency_symbol    = "",
        .frac_digits        = CHAR_MAX,
        .p_cs_precedes      = CHAR_MAX,
        .n_cs_precedes      = CHAR_MAX,
        .p_sep_by_space     = CHAR_MAX,
        .n_sep_by_space     = CHAR_MAX,
        .p_sign_posn        = CHAR_MAX,
        .n_sign_posn        = CHAR_MAX,
        .int_curr_symbol    = "",
        .int_frac_digits    = CHAR_MAX,
        .int_p_cs_precedes  = CHAR_MAX,
        .int_n_cs_precedes  = CHAR_MAX,
        .int_p_sep_by_space = CHAR_MAX,
        .int_n_sep_by_space = CHAR_MAX,
        .int_p_sign_posn    = CHAR_MAX,
        .int_n_sign_posn    = CHAR_MAX
    },
    {'C', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}
};

locale_t* _Nonnull   __CUR_LC;           // qe is always NULL
SList                __FIRST_LIBC_LC;    // libc defined locales
SList                __FIRST_USER_LC;    // user defined locales
struct lconv         __TMP_LCONV;        // used as a temp buffer
unsigned int         __UNIQUE_ID_LC;        // unique id used as the name of a user defined locale
mtx_t                __MTX_LC;


void __locale_init(void)
{
    __UNIQUE_ID_LC = 1;

    __FIRST_LIBC_LC.first = &__LOCALE_C.qe;
    __FIRST_LIBC_LC.last  = &__LOCALE_C.qe;
    __FIRST_USER_LC = SLIST_INIT;
    __CUR_LC = &__LOCALE_C;

    mtx_init(&__MTX_LC);
}
