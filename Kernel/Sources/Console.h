//
//  Console.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include <klib/klib.h>
#include "EventDriver.h"
#include "Geometry.h"
#include "GraphicsDriver.h"
#include "KeyMap.h"
#include "Lock.h"


typedef enum _LineBreakMode {
    kLineBreakMode_Clip = 0,
    kLineBreakMode_WrapCharacter
} LineBreakMode;


// If set then the family of console print() functions automatically scrolls the
// console up if otherwise the function would end up printing below the bottom
// edge of the console screen.
#define CONSOLE_FLAG_AUTOSCROLL_TO_BOTTOM   0x01


typedef struct _Console {
    EventDriverRef _Nonnull     pEventDriver;
    GraphicsDriverRef _Nonnull  pGDevice;
    Int8                        x;
    Int8                        y;
    Int8                        cols;       // 80
    Int8                        rows;       // 25
    UInt8                       flags;
    Int8                        lineBreakMode;
    Int8                        tabWidth;   // 8
    Lock                        lock;
    const KeyMap* _Nonnull      keyMap;
    ByteCount                   keyMapBufferCapacity;
    Byte* _Nonnull              keyMapBuffer;
} Console;


extern ErrorCode Console_Create(EventDriverRef _Nonnull pEventDriver, GraphicsDriverRef _Nonnull pGDevice, Console* _Nullable * _Nonnull pOutConsole);
extern void Console_Destroy(Console* _Nullable pConsole);

extern Rect Console_GetBounds(Console* _Nonnull pConsole);

extern void Console_ClearScreen(Console* _Nonnull pConsole);
extern void Console_ClearLine(Console* _Nonnull pConsole, Int y);

extern void Console_CopyRect(Console* _Nonnull pConsole, Rect srcRect, Point dstLoc);
extern void Console_FillRect(Console* _Nonnull pConsole, Rect rect, Character ch);
extern void Console_ScrollBy(Console* _Nonnull pConsole, Rect clipRect, Point dXY);

extern void Console_MoveCursor(Console* _Nonnull pConsole, Int dx, Int dy);
extern void Console_MoveCursorTo(Console* _Nonnull pConsole, Int x, Int y);

extern ByteCount Console_Write(Console* _Nonnull pConsole, const Byte* _Nonnull pBytes, ByteCount nBytesToWrite);
extern ByteCount Console_Read(Console* _Nonnull pConsole, Byte* _Nonnull pBuffer, ByteCount nBytesToRead);

#endif /* Console_h */
