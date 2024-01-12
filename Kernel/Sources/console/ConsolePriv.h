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
#include "vtparser.h"


//
// Console/Terminal related reference documentation:
//
// <https://vt100.net/docs/vt510-rm/chapter4.html>
// <https://vt100.net/annarbor/aaa-ug/section6.html>
// <https://vt100.net/emu/dec_ansi_parser>
// <https://en.wikipedia.org/wiki/ANSI_escape_code>
// <https://en.wikipedia.org/wiki/VT52>
// <https://nvlpubs.nist.gov/nistpubs/Legacy/FIPS/fipspub86.pdf>
// <https://noah.org/python/pexpect/ANSI-X3.64.htm>
//


typedef enum _CompatibilityMode {
    kCompatibilityMode_VT52 = 0,
    kCompatibilityMode_VT52_AtariExtensions,
    kCompatibilityMode_VT100
} CompatibilityMode;
#define kCompatibilityMode_ANSI kCompatibilityMode_VT100


// Unimplemented functionality:
//  VT100:
//      - bell
//      - shift in/out (character set selection)
//      - answerback message
//      - (DECSCLM) animated vs non-animated scroll
//      - (DECSTBM) scroll region
//      - (DECOM) origin
//      - (DECCOLM) columns per line (80 vs 132)
//      - (DECSCNM) screen background
//      - (LNM) linefeed/new line mode
//      - (KAM) keyboard action mode (questionable)
//      - (DECARM) auto repeat mode
//      - (SRM) local echo
//      - (DECCKM) cursor key mode
//      - (DECKPAM) application key mode
//      - (DECKPNM) numeric keypad mode
//      - (SCS) select character set
//      - (SGR) select graphic rendition
//      - (DECDHL) double-height line
//      - (DECSWL) single-width line
//      - (DECDWL) double-width line
//      - (MC) media copy
//      - (DECPEX) printer extent mode
//      - (DECPFF) print termination character
//      - (DECTST) tests
//      - (DECALN) screen alignment display
//      - (DECLL) keyboard indicator
//
//  VT52:
//      - (ESC =) keypad character selection
//      - (ESC >) keypad character selection
//      - (ESC F) enter graphics mode
//      - (ESC G) exit graphics mode
//      - (ESC ^) auto print on
//      - (ESC _) auto print off
//      - (ESC W) print controller on
//      - (ESC X) print controller off
//      - (ESC V) print cursor line
//      - (ESC ]) print screen
//
// VT52 Atari Extensions:
//      - (ESC b) foreground color
//      - (ESC c) background color
//      - (ESC p) reverse video
//      - (ESC q) normal video


typedef enum _CursorMovement {
    kCursorMovement_Clamp = 0,      // Insertion point movement is restricted to the screen area. No scrolling our auto-wrap is done.
    kCursorMovement_AutoWrap,       // Move the insertion point to the beginning of the next line if it moves past the right margin and scroll the screen content up a line if it moves past the bottom margin
    kCursorMovement_AutoScroll      // Horizontal insertion pointer is clamped and vertical movement will scroll the screen up/down if the insertion pointer moves past teh bottom/top edge of the screen
} CursorMovement;


// The values are chosen based on the ANSI EL command
typedef enum _ClearLineMode {
    kClearLineMode_ToEnd = 0,           // Clear from the cursor position to the end of the line
    kClearLineMode_ToBeginning = 1,     // Clear from the cursor position to the beginning of the line
    kClearLineMode_Whole = 2,           // Clear the whole line
} ClearLineMode;


// The values are chosen based on the ANSI ED command
typedef enum _ClearScreenMode {
    kClearScreenMode_ToEnd = 0,             // Clear from the cursor position to the end of the screen
    kClearScreenMode_ToBeginning = 1,       // Clear from the cursor position to the beginning of the screen
    kClearScreenMode_Whole = 2,             // Clear the whole screen
    kClearScreenMode_WhileAndScrollback = 3 // Clear the whole screen and the scrollback buffer
} ClearScreenMode;


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


// Saved cursor state:
// - cursor position
// - cursor attributes
// - character set
// - origin mode
typedef struct _SavedState {
    Int     x;
    Int     y;
} SavedState;


// The console object.
CLASS_IVARS(Console, IOResource,
    Lock                        lock;
    EventDriverRef _Nonnull     eventDriver;
    IOChannelRef _Nonnull       eventDriverChannel;
    GraphicsDriverRef _Nonnull  gdevice;
    RingBuffer                  reportsQueue;
    RGBColor                    backgroundColor;
    RGBColor                    textColor;
    Int                         lineHeight;     // In pixels
    Int                         characterWidth; // In pixels
    TabStops                    hTabStops;
    Rect                        bounds;
    Int                         x;
    Int                         y;
    SavedState                  savedCursorState;
    vtparser_t                  vtparser;
    SpriteID                    textCursor;
    TimerRef _Nonnull           textCursorBlinker;
    CompatibilityMode           compatibilityMode;
    struct {
        UInt    isAutoWrapEnabled: 1;   // true if the cursor should move to the next line if printing a character would move it past teh right margin
        UInt    isInsertionMode: 1;     // true if insertion mode is active; false if replace mode is active

        UInt    isTextCursorBlinkerEnabled:1;   // true if the text cursor should blink. Visibility is a separate state
        UInt    isTextCursorOn:1;               // true if the text cursor blinking state is on; false if off. IsTextCursorVisible has to be true to make the cursor actually visible
        UInt    isTextCursorSingleCycleOn:1;    // true if the text cursor should be shown for a single blink cycle even if the cycle is actually supposed to be off. This is set when we print a character to ensure the cursor is visible
        UInt    isTextCursorVisible:1;          // global text cursor visibility switch
    }                           flags;
);


// Big enough to hold the result of a key mapping and the longest possible
// terminal report message.
#define MAX_MESSAGE_LENGTH kKeyMap_MaxByteSequenceLength

// Takes care of mapping a USB key scan code to a character or character sequence.
// We may leave partial character sequences in the buffer if a Console_Read() didn't
// read all bytes of a sequence. The next Console_Read() will first receive the
// remaining buffered bytes before it receives bytes from new events.
OPEN_CLASS_WITH_REF(ConsoleChannel, IOChannel,
    const KeyMap* _Nonnull  map;
    Byte                    buffer[MAX_MESSAGE_LENGTH];  // Holds a full or partial byte sequence produced by a key down event
    Int8                    count;      // Number of bytes stored in the buffer
    Int8                    startIndex; // Index of first byte in the buffer where a partial byte sequence begins
);
typedef struct _ConsoleChannelMethodTable {
    IOChannelMethodTable    super;
} ConsoleChannelMethodTable;


extern ErrorCode Console_ResetState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_ClearScreen_Locked(Console* _Nonnull pConsole, ClearScreenMode mode);
extern void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, Int y, ClearLineMode mode);
extern void Console_SaveCursorState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_SetCursorBlinkingEnabled_Locked(Console* _Nonnull pConsole, Bool isEnabled);
extern void Console_SetCursorVisible_Locked(Console* _Nonnull pConsole, Bool isVisible);
static void Console_OnTextCursorBlink(Console* _Nonnull pConsole);
extern void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, Int x, Int y);
extern void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, CursorMovement mode, Int dx, Int dy);
extern void Console_SetCompatibilityMode(ConsoleRef _Nonnull pConsole, CompatibilityMode mode);
extern void Console_VT52_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt52parse_action_t action, unsigned char b);
extern void Console_VT100_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt500parse_action_t action, unsigned char b);

extern void Console_PostReport_Locked(ConsoleRef _Nonnull pConsole, const Character* msg);

extern void Console_PrintByte_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch);
extern void Console_Execute_BEL_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_HT_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_LF_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_BS_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_DEL_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_DCH_Locked(ConsoleRef _Nonnull pConsole, Int nChars);
extern void Console_Execute_IL_Locked(ConsoleRef _Nonnull pConsole, Int nLines);
extern void Console_Execute_DL_Locked(ConsoleRef _Nonnull pConsole, Int nLines);


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
