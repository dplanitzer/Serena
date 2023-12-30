//
//  LineReader.h
//  sh
//
//  Created by Dietmar Planitzer on 12/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <apollo/apollo.h>


typedef struct _LineReader {
    int     x;
    int     maxX;

    int     lineCapacity;
    int     lineCount;

    char    line[1];
} LineReader;
typedef LineReader* LineReaderRef;


extern errno_t LineReader_Create(int maxLineLength, LineReaderRef _Nullable * _Nonnull pOutReader);
extern void LineReader_Destroy(LineReaderRef _Nullable self);

extern char* _Nonnull LineReader_ReadLine(LineReaderRef _Nonnull self);
