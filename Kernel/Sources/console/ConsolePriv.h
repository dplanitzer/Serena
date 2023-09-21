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
#include "vtparse.h"


typedef enum _LineBreakMode {
    kLineBreakMode_Clip = 0,
    kLineBreakMode_WrapCharacter
} LineBreakMode;


typedef struct _KeyMapper {
    const KeyMap* _Nonnull  map;
    Byte * _Nonnull         buffer;     // Holds a full or partial byte sequence produced by a key down event
    ByteCount               capacity;   // Maximum number of bytes the buffer can hold
    ByteCount               count;      // Number of bytes stored in the buffer
    Int                     startIndex; // Index of first byte in the buffer where a partial byte sequence begins
} KeyMapper;

typedef struct _Console {
    Lock                        lock;
    EventDriverRef _Nonnull     pEventDriver;
    GraphicsDriverRef _Nonnull  pGDevice;
    Rect                        bounds;
    Int8                        x;
    Int8                        y;
    Int8                        tabWidth;   // 8
    Int8                        lineBreakMode;
    Bool                        isAutoScrollEnabled;
    vtparse_t                   vtparse;
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
