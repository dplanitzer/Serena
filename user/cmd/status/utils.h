//
//  utils.h
//  status
//
//  Created by Dietmar Planitzer on 4/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <_cmndef.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ext/nanotime.h>

#define FMT_MEM_SIZE_BUFFER_SIZE    __LLONG_MAX_BASE_10_DIGITS + 1
#define FMT_DURATION_BUFFER_SIZE    9

extern void term_cursor_on(bool onOff);
extern void term_cls(void);
extern void term_move_to(int x, int y);

extern char* _Nonnull fmt_mem_size(uint64_t msize, char* _Nonnull buf);
extern char* _Nonnull fmt_duration(const nanotime_t* _Nonnull dur, char* _Nonnull buf);
