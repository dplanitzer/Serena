//
//  ConsolePriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConsolePriv_h
#define ConsolePriv_h

#include "Console.h"
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


typedef struct _KeyMapper {
    const KeyMap* _Nonnull  map;
    Byte * _Nonnull         buffer;     // Holds a full or partial byte sequence produced by a key down event
    ByteCount               capacity;   // Maximum number of bytes the buffer can hold
    ByteCount               count;      // Number of bytes stored in the buffer
    Int                     startIndex; // Index of first byte in the buffer where a partial byte sequence begins
} KeyMapper;

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
    KeyMapper                   keyMapper;
} Console;


//
// Keymaps
//
extern const unsigned char gKeyMap_usa[];


//
// Fonts
//
extern const Byte font8x8_latin1[128][8];
extern const Byte font8x8_dingbat[160][8];
#define GLYPH_WIDTH     8
#define GLYPH_HEIGHT    8

#endif /* ConsolePriv_h */
