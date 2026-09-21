//
//  readline.h
//  readline
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _READLINE_H_
#define _READLINE_H_

#include <stddef.h>


#define RL_SCREEN_WIDTH   -1

struct readline;
typedef struct readline* rl_t;


// Create a new line reader. The line reader spans a single row which shows the
// prompt on the left side and an input field to the right of the prompt. The
// left edge of the prompt appears at 'x' and the line reader is 'width' columns
// wide (prompt + input line length). Pass RL_SCREEN_WIDTH as 'width' to
// make the line reader as wide as the screen. Note that 'x' is zero based.
extern rl_t _Nonnull rl_create(int x, int width);
extern void rl_destroy(rl_t _Nullable self);

extern char* _Nonnull rl_readline(rl_t _Nonnull self);

extern void rl_setprompt(rl_t _Nonnull self, const char* _Nonnull str);
extern void rl_sethistorycapacity(rl_t _Nonnull self, size_t capacity);

extern int rl_historycount(rl_t _Nonnull self);
extern const char* _Nonnull rl_historyat(rl_t _Nonnull self, int idx);

#endif  /* _READLINE_H_ */
