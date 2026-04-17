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

extern void term_cursor_on(bool onOff);
extern void term_cls(void);
extern void term_move_to(int x, int y);

extern char* fmt_mem_size(uint64_t msize, char* _Nonnull buf);
