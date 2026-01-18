//
//  _imaxabsdiv.h
//  libc
//
//  Created by Dietmar Planitzer on 12/26/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef __IMAXABSDIV_H
#define __IMAXABSDIV_H 1

#include <_cmndef.h>
#include <stdint.h>

__CPP_BEGIN

typedef struct imaxdiv_t { intmax_t quot; intmax_t rem; } imaxdiv_t;

extern intmax_t imaxabs(intmax_t n);
extern imaxdiv_t imaxdiv(intmax_t x, intmax_t y);


typedef struct uimaxdiv_t { uintmax_t quot; uintmax_t rem; } uimaxdiv_t;

extern uimaxdiv_t uimaxdiv(uintmax_t x, uintmax_t y);

__CPP_END

#endif /* __IMAXABSDIV_H */
