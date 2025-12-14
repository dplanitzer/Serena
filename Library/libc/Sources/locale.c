//
//  locale.c
//  libc
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <locale.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mtx.h>
#include <sys/queue.h>


// Locale name:
// - 'C', 'GER-GER', 'US-EN', etc for system defined locales
// - '%'<unique id> for user defined locales (unique id length is 8 chars)
#define __MAX_LOCALE_NAME_LENGTH    10
typedef struct locale {
    SListNode       qe;
    struct lconv    lc;
    char            name[__MAX_LOCALE_NAME_LENGTH];
} locale_t;


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

static locale_t* _Nonnull   __CUR_LC;           // qe is always NULL
static SList                __FIRST_LIBC_LC;    // libc defined locales
static SList                __FIRST_USER_LC;    // user defined locales
static struct lconv         __TMP_LCONV;        // used as a temp buffer
static unsigned int         __UNIQUE_ID;        // unique id used as the name of a user defined locale
static mtx_t                __MTX;


void __locale_init(void)
{
    __UNIQUE_ID = 1;

    __FIRST_LIBC_LC.first = &__LOCALE_C.qe;
    __FIRST_LIBC_LC.last  = &__LOCALE_C.qe;
    __FIRST_USER_LC = SLIST_INIT;
    __CUR_LC = &__LOCALE_C;

    mtx_init(&__MTX);
}


static locale_t* _Nullable __get_locale_by_name(const char* _Nonnull locale)
{
    if (*locale == '\0' || (*locale == 'P' && !strcmp(locale, "POSIX"))) {
        locale = "C";
    }

    SList* table = (*locale == '%') ? &__FIRST_USER_LC : &__FIRST_LIBC_LC;
    SList_ForEach(table, locale_t, {
        if (!strcmp(locale, pCurNode->name)) {
            return pCurNode;
        }
    })

    return NULL;
}

static int __lconvcmp(const struct lconv* _Nonnull ll, const struct lconv* _Nonnull rl)
{
    if (strcmp(ll->decimal_point, rl->decimal_point)) {
        return 1;
    }
    if (strcmp(ll->thousands_sep, rl->thousands_sep)) {
        return 1;
    }
    if (strcmp(ll->grouping, rl->grouping)) {
        return 1;
    }
    if (strcmp(ll->mon_decimal_point, rl->mon_decimal_point)) {
        return 1;
    }
    if (strcmp(ll->mon_thousands_sep, rl->mon_thousands_sep)) {
        return 1;
    }
    if (strcmp(ll->mon_grouping, rl->mon_grouping)) {
        return 1;
    }
    if (strcmp(ll->positive_sign, rl->positive_sign)) {
        return 1;
    }
    if (strcmp(ll->negative_sign, rl->negative_sign)) {
        return 1;
    }
    if (strcmp(ll->currency_symbol, rl->currency_symbol)) {
        return 1;
    }
    if (ll->frac_digits != rl->frac_digits) {
        return 1;
    }
    if (ll->p_cs_precedes != rl->p_cs_precedes) {
        return 1;
    }
    if (ll->n_cs_precedes != rl->n_cs_precedes) {
        return 1;
    }
    if (ll->p_sep_by_space != rl->p_sep_by_space) {
        return 1;
    }
    if (ll->n_sep_by_space != rl->n_sep_by_space) {
        return 1;
    }
    if (ll->p_sign_posn != rl->p_sign_posn) {
        return 1;
    }
    if (ll->n_sign_posn != rl->n_sign_posn) {
        return 1;
    }
    if (strcmp(ll->int_curr_symbol, rl->int_curr_symbol)) {
        return 1;
    }
    if (ll->int_frac_digits != rl->int_frac_digits) {
        return 1;
    }
    if (ll->int_p_cs_precedes != rl->int_p_cs_precedes) {
        return 1;
    }
    if (ll->int_n_cs_precedes != rl->int_n_cs_precedes) {
        return 1;
    }
    if (ll->int_p_sep_by_space != rl->int_p_sep_by_space) {
        return 1;
    }
    if (ll->int_n_sep_by_space != rl->int_n_sep_by_space) {
        return 1;
    }
    if (ll->int_p_sign_posn != rl->int_p_sign_posn) {
        return 1;
    }
    if (ll->int_n_sign_posn != rl->int_n_sign_posn) {
        return 1;
    }

    return 0;
}

static locale_t* _Nullable __get_locale_by_lconv(const struct lconv* _Nonnull lconv)
{
    SList_ForEach(&__FIRST_USER_LC, locale_t, {
        const struct lconv* cl = &pCurNode->lc;

        if (!__lconvcmp(cl, lconv)) {
            return pCurNode;
        }
    })

    return NULL;
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


static locale_t* _Nullable __makelocale(int category, const locale_t* _Nonnull baseLocale, const locale_t* _Nonnull otherLocale)
{
    const struct lconv* sl = &otherLocale->lc;
    struct lconv* dl = &__TMP_LCONV;

    memcpy(dl, baseLocale, sizeof(struct lconv));
    switch (category) {
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


    locale_t* existingLocale = __get_locale_by_lconv(dl);
    if (existingLocale) {
        return existingLocale;
    }


    locale_t* newLocale = malloc(sizeof(locale_t));
    if (newLocale == NULL) {
        return NULL;
    }

    newLocale->qe = SLISTNODE_INIT;
    memcpy(&newLocale->lc, dl, sizeof(struct lconv));

    newLocale->name[0] = '%';
    itoa(__UNIQUE_ID, &newLocale->name[1], 16);
    newLocale->name[__MAX_LOCALE_NAME_LENGTH - 1] = '\0';

    SList_InsertBeforeFirst(&__FIRST_USER_LC, &newLocale->qe);

    return newLocale;
}

char* _Nullable setlocale(int category, const char* _Nullable locale)
{
    char* r = NULL;

    mtx_lock(&__MTX);

    if (locale == NULL) {
        r = __CUR_LC->name;
    }
    else {
        locale_t* sl = __get_locale_by_name(locale);
        
        if (sl) {
            if (category == LC_ALL) {
                __CUR_LC = sl;
                r = __CUR_LC->name;
            }
            else {
                locale_t* nl = __makelocale(category, __CUR_LC, sl);

                if (nl) {
                    __CUR_LC = nl;
                    r = __CUR_LC->name;
                }
            }
        }
    }

    mtx_unlock(&__MTX);

    return r;
}

struct lconv * _Nonnull localeconv(void)
{
    mtx_lock(&__MTX);
    struct lconv* r = &__CUR_LC->lc;
    mtx_unlock(&__MTX);
    return r;
}
