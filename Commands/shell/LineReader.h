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
#include <sys/errno.h>


typedef struct LineReader {
    int     x;
    int     maxX;

    const char* prompt;
    char*   savedLine;  // Line saved if 'line' was dirty when user hits crsr-up/down
    bool    isDirty;

    char**  history;
    int     historyCapacity;
    int     historyCount;
    int     historyIndex;

    int     lineCapacity;
    int     lineCount;
    char    line[1];
} LineReader;
typedef LineReader* LineReaderRef;


extern errno_t LineReader_Create(int maxLineLength, int historyCapacity, const char* _Nonnull pPrompt, LineReaderRef _Nullable * _Nonnull pOutReader);
extern void LineReader_Destroy(LineReaderRef _Nullable self);

extern char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self);

extern int LineReader_GetHistoryCount(LineReaderRef _Nonnull self);
extern const char* _Nonnull LineReader_GetHistoryAt(LineReaderRef _Nonnull self, int idx);

#endif  /* LineReader_h */
