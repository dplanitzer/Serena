//
//  __readline.h
//  readline
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __READLINE_H_
#define __READLINE_H_

#include <readline.h>
#include <stdbool.h>

struct readline {
    // Prompt
    char*   prompt;
    size_t  promptLength;
    size_t  promptCapacity;

    // Input line
    char*   line;               // has an extra character for the terminating NUL
    int     lineCapacity;
    int     lineLastCol;        // last column in line buffer that can hold data
    int     textLastCol;        // last column of what the user has entered so far
    int     cursorX;            // current cursor X position in line

    // Geometry (everything is zero based)
    int     lrX;
    int     lrY;                // initialized by CalcLayout()
    int     lrWidth;
    int     promptX;
    int     promptWidth;
    int     inputAreaFirstCol;

    // History buffer
    char*   savedLine;  // Line saved if 'line' was dirty when user hits crsr-up/down
    bool    isDirty;

    char**  history;
    int     historyCapacity;
    int     historyCount;
    int     historyIndex;

    // Editor modes
    struct __Flags {
        unsigned int    isInsertMode:1;
        unsigned int    hasTermInsertMode:1;
        unsigned int    reserved:30;
    }       flags;
};


#endif  /* __READLINE_H_ */
