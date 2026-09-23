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


#define RL_CREATE_STRUCT_TAG    0

// Line reader input line should be as wide as the terminal screen
#define RL_SCREEN_WIDTH -1


typedef struct rl_create_info {
    int                     tag;            // RL_CREATE_STRUCT_TAG
    unsigned int            flags;
    size_t                  max_history_count;
    int                     x;
    int                     width;
    const char* _Nullable   prompt;
} rl_create_info_t;


struct readline;
typedef struct readline* rl_t;


// Create a new line reader. The line reader spans a single row which shows the
// prompt on the left side and an input field to the right of the prompt. The
// left edge of the prompt appears at 'x' and the line reader is 'width' columns
// wide (prompt + input line length). Pass RL_SCREEN_WIDTH as 'width' to
// make the line reader as wide as the screen. Note that 'x' is zero based.
extern rl_t _Nullable rl_create(const rl_create_info_t* _Nonnull info);

// Frees all resources allocated by 'self'.
extern void rl_destroy(rl_t _Nullable self);


// Shows the prompt in the line in which the cursor currently resides and then
// blocks the caller until the user has entered a line and pressed the return
// or enter key. This function returns a pointer to the string that the user
// has entered. The caller should copy the string and not manipulate it.
extern const char* _Nonnull rl_readline(rl_t _Nonnull self);


// Sets the prompt to the string 'str'. The string is copied. Note that the
// string may contain escape sequences.
extern void rl_set_prompt(rl_t _Nonnull self, const char* _Nonnull str);


// Returns the number of entries currently stored in the history.
extern size_t rl_history_count(rl_t _Nonnull self);

// Returns a read-only reference to the string that represents the 'idx' history
// entry. The caller should copy this string.
extern const char* _Nullable rl_history_at(rl_t _Nonnull self, size_t idx);

#endif  /* _READLINE_H_ */
