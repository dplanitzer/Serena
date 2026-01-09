//
//  _locale.h
//  libc
//
//  Created by Dietmar Planitzer on 12/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __LOCALE_H
#define __LOCALE_H 1

#include <locale.h>
#include <ext/queue.h>
#include <sys/mtx.h>

__CPP_BEGIN

// Locale name:
// - 'C', 'GER-GER', 'US-EN', etc for system defined locales
// - '%'<unique id> for user defined locales (unique id length is 8 chars)
#define __MAX_LOCALE_NAME_LENGTH    10
typedef struct locale {
    queue_node_t    qe;
    struct lconv    lc;
    char            name[__MAX_LOCALE_NAME_LENGTH];
} locale_t;


extern locale_t* _Nonnull   __CUR_LC;           // qe is always NULL
extern queue_t              __FIRST_LIBC_LC;    // libc defined locales
extern queue_t              __FIRST_USER_LC;    // user defined locales
extern struct lconv         __TMP_LCONV;        // used as a temp buffer
extern unsigned int         __UNIQUE_ID_LC;        // unique id used as the name of a user defined locale
extern mtx_t                __MTX_LC;


extern void __locale_init(void);

__CPP_END

#endif /* __LOCALE_H */
