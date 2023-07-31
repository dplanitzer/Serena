//
//  Console.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Console_h
#define Console_h

#include "Foundation.h"
#include "Geometry.h"
#include "GraphicsDriver.h"
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
    GraphicsDriverRef _Nonnull  pGDevice;
    Int8                        x;
    Int8                        y;
    Int8                        cols;       // 80
    Int8                        rows;       // 25
    UInt8                       flags;
    Int8                        lineBreakMode;
    Int8                        tabWidth;   // 8
    Lock                        lock;
} Console;


extern Console* _Nullable   gConsole;

extern ErrorCode Console_Create(GraphicsDriverRef _Nonnull pGDevice, Console* _Nullable * _Nonnull pOutConsole);
extern void Console_Destroy(Console* _Nullable pConsole);

extern ErrorCode Console_GetBounds(Console* _Nonnull pConsole, Rect* _Nonnull pOutRect);

extern ErrorCode Console_ClearScreen(Console* _Nonnull pConsole);
extern ErrorCode Console_ClearLine(Console* _Nonnull pConsole, Int y);

extern ErrorCode Console_CopyRect(Console* _Nonnull pConsole, Rect srcRect, Point dstLoc);
extern ErrorCode Console_FillRect(Console* _Nonnull pConsole, Rect rect, Character ch);
extern ErrorCode Console_ScrollBy(Console* _Nonnull pConsole, Rect clipRect, Point dXY);

extern ErrorCode Console_MoveCursor(Console* _Nonnull pConsole, Int dx, Int dy);
extern ErrorCode Console_MoveCursorTo(Console* _Nonnull pConsole, Int x, Int y);

extern ErrorCode Console_DrawCharacter(Console* _Nonnull pConsole, Character ch);
extern ErrorCode Console_DrawString(Console* _Nonnull pConsole, const Character* _Nonnull str);

#endif /* Console_h */
