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
#include "DispatchQueue.h"
#include "KeyMap.h"
#include "Lock.h"
#include "vtparse.h"


//
// Console/Terminal related reference documentation:
//
// <https://vt100.net/docs/vt510-rm/chapter4.html>
// <https://vt100.net/annarbor/aaa-ug/section6.html>
// <https://vt100.net/emu/dec_ansi_parser>
// <https://en.wikipedia.org/wiki/ANSI_escape_code>
// <https://nvlpubs.nist.gov/nistpubs/Legacy/FIPS/fipspub86.pdf>
// <https://noah.org/python/pexpect/ANSI-X3.64.htm>
//


typedef enum _LineBreakMode {
    kLineBreakMode_Clamp = 0,       // Force the insertion point to the last character cell in the line, if it would move past the right margin
    kLineBreakMode_WrapCharacter,   // Move the insertion point to the beginning of the next line if it moves past the right margin. Force the
                                    // insertion point to the last character cell of the last line if it would move past the bottom margin
    kLineBreakMode_WrapCharacterAndScroll   // Move the insertion point to the beginning of the next line if it moves past the right margin and
                                            // scroll the screen content up one line if it moves past the bottom margin
} LineBreakMode;

// The values are chosen based on the EL (Erase in Line) CSI (CSI n K)
typedef enum _ClearLineMode {
    kClearLineMode_ToEnd = 0,           // Clear from the cursor position to the end of the line
    kClearLineMode_ToBeginning = 1,     // Clear from the cursor position to the beginning of the line
    kClearLineMode_Whole = 2,           // Clear whole line
} ClearLineMode;


// Stores up to 255 tab stop positions.
typedef struct _TabStops {
    Int8*   stops;
    Int     count;
    Int     capacity;
} TabStops;

ErrorCode TabStops_Init(TabStops* _Nonnull pStops, Int nStops, Int tabWidth);
void TabStops_Deinit(TabStops* _Nonnull pStops);
ErrorCode TabStops_InsertStop(TabStops* _Nonnull pStops, Int xLoc);
void TabStops_RemoveStop(TabStops* _Nonnull pStops, Int xLoc);
void TabStops_RemoveAllStops(TabStops* _Nonnull pStops);
Int TabStops_GetNextStop(TabStops* pStops, Int xLoc, Int xWidth);
Int TabStops_GetNextNthStop(TabStops* pStops, Int xLoc, Int nth, Int xWidth);
Int TabStops_GetPreviousNthStop(TabStops* pStops, Int xLoc, Int nth);


// Takes care of mapping a USB key scan code to a character or character sequence.
// We may leave partial character sequences in the buffer if a Console_Read() didn't
// read all bytes of a sequence. The next Console_Read() will first receive the
// remaining buffered bytes before it receives bytes from new events.
typedef struct _KeyMapper {
    const KeyMap* _Nonnull  map;
    Byte * _Nonnull         buffer;     // Holds a full or partial byte sequence produced by a key down event
    ByteCount               capacity;   // Maximum number of bytes the buffer can hold
    ByteCount               count;      // Number of bytes stored in the buffer
    Int                     startIndex; // Index of first byte in the buffer where a partial byte sequence begins
} KeyMapper;


// The console object.
typedef struct _Console {
    Lock                        lock;
    EventDriverRef _Nonnull     pEventDriver;
    GraphicsDriverRef _Nonnull  pGDevice;
    RGBColor                    backgroundColor;
    RGBColor                    textColor;
    Int                         lineHeight;     // In pixels
    Int                         characterWidth; // In pixels
    TabStops                    hTabStops;
    TabStops                    vTabStops;
    Rect                        bounds;
    Int                         x;
    Int                         y;
    LineBreakMode               lineBreakMode;
    Point                       savedCursorPosition;
    vtparse_t                   vtparse;
    KeyMapper                   keyMapper;
    SpriteID                    textCursor;
    TimerRef _Nonnull           textCursorBlinker;
    Bool                        isTextCursorBlinkerActive;
    Bool                        isTextCursorVisible;
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


//
// Text Cursors
//
extern const UInt16 gBlock4x8_Plane0[];
extern const UInt16 gBlock4x8_Plane1[];
extern const Int gBlock4x8_Width;
extern const Int gBlock4x8_Height;

extern const UInt16 gBlock4x4_Plane0[];
extern const UInt16 gBlock4x4_Plane1[];
extern const Int gBlock4x4_Width;
extern const Int gBlock4x4_Height;

#endif /* ConsolePriv_h */
