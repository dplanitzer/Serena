//
//  locale.h
//  libc
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _LOCALE_H
#define _LOCALE_H 1

#include <System/_cmndef.h>
#include <System/_nulldef.h>

__CPP_BEGIN

struct lconv {
    char * _Nonnull decimal_point;
    char * _Nonnull thousands_sep;
    char * _Nonnull grouping;
    char * _Nonnull mon_decimal_point;
    char * _Nonnull mon_thousands_sep;
    char * _Nonnull mon_grouping;
    char * _Nonnull positive_sign;
    char * _Nonnull negative_sign;
    char * _Nonnull currency_symbol;
    char            frac_digits;
    char            p_cs_precedes;
    char            n_cs_precedes;
    char            p_sep_by_space;
    char            n_sep_by_space;
    char            p_sign_posn;
    char            n_sign_posn;
    char * _Nonnull int_curr_symbol;
    char            int_frac_digits;
    char            int_p_cs_precedes;
    char            int_n_cs_precedes;
    char            int_p_sep_by_space;
    char            int_n_sep_by_space;
    char            int_p_sign_posn;
    char            int_n_sign_posn;
};


#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5
#define _LC_LAST    LC_TIME


extern char* _Nonnull setlocale(int category, const char* _Nullable locale);
extern struct lconv * _Nonnull localeconv(void);

__CPP_END

#endif /* _LOCALE_H */
