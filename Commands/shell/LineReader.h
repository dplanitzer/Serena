//
//  LineReader.h
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef LineReader_h
#define LineReader_h

#include <stdbool.h>
#include <stddef.h>


#define kLineReader_ScreenWidth   -1

typedef struct LineReader {
    int     fd_in;
    int     fd_out;

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
} LineReader;
typedef LineReader* LineReaderRef;


// Create a new line reader. The line reader spans a single row which shows the
// prompt on the left side and an input field to the right of the prompt. The
// left edge of the prompt appears at 'x' and the line reader is 'width' columns
// wide (prompt + input line length). Pass kLineReader_ScreenWidth as 'width' to
// make the line reader as wide as the screen. Note that 'x' is zero based.
extern LineReaderRef _Nonnull LineReader_Create(int x, int width);
extern void LineReader_Destroy(LineReaderRef _Nullable self);

extern char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self);

extern void LineReader_SetPrompt(LineReaderRef _Nonnull self, const char* _Nonnull str);
extern void LineReader_SetHistoryCapacity(LineReaderRef _Nonnull self, size_t capacity);

extern int LineReader_GetHistoryCount(LineReaderRef _Nonnull self);
extern const char* _Nonnull LineReader_GetHistoryAt(LineReaderRef _Nonnull self, int idx);

#endif  /* LineReader_h */
