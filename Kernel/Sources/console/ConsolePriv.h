//
//  ConsolePriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConsolePriv_h
#define ConsolePriv_h

#include "Console.h"
#include <dispatcher/Lock.h>
#include <dispatchqueue/DispatchQueue.h>
#include "KeyMap.h"
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


typedef enum CompatibilityMode {
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
//      - (DECDHL) double-height line
//      - (DECSWL) single-width line
//      - (DECDWL) double-width line
//      - (MC) media copy
//      - (DECPEX) printer extent mode
//      - (DECPFF) print termination character
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


typedef enum CursorMovement {
    kCursorMovement_Clamp = 0,      // Insertion point movement is restricted to the screen area. No scrolling our auto-wrap is done.
    kCursorMovement_AutoWrap,       // Move the insertion point to the beginning of the next line if it moves past the right margin and scroll the screen content up a line if it moves past the bottom margin
    kCursorMovement_AutoScroll      // Horizontal insertion pointer is clamped and vertical movement will scroll the screen up/down if the insertion pointer moves past teh bottom/top edge of the screen
} CursorMovement;


// The values are chosen based on the ANSI EL command
typedef enum ClearLineMode {
    kClearLineMode_ToEnd = 0,           // Clear from the cursor position to the end of the line
    kClearLineMode_ToBeginning = 1,     // Clear from the cursor position to the beginning of the line
    kClearLineMode_Whole = 2,           // Clear the whole line
} ClearLineMode;


// The values are chosen based on the ANSI ED command
typedef enum ClearScreenMode {
    kClearScreenMode_ToEnd = 0,             // Clear from the cursor position to the end of the screen
    kClearScreenMode_ToBeginning = 1,       // Clear from the cursor position to the beginning of the screen
    kClearScreenMode_Whole = 2,             // Clear the whole screen
    kClearScreenMode_WhileAndScrollback = 3 // Clear the whole screen and the scrollback buffer
} ClearScreenMode;


// Stores up to 255 tab stop positions.
typedef struct TabStops {
    int8_t* stops;
    int     count;
    int     capacity;
} TabStops;

errno_t TabStops_Init(TabStops* _Nonnull pStops, int nStops, int tabWidth);
void TabStops_Deinit(TabStops* _Nonnull pStops);
errno_t TabStops_InsertStop(TabStops* _Nonnull pStops, int xLoc);
void TabStops_RemoveStop(TabStops* _Nonnull pStops, int xLoc);
void TabStops_RemoveAllStops(TabStops* _Nonnull pStops);
int TabStops_GetNextStop(TabStops* pStops, int xLoc, int xWidth);
int TabStops_GetNextNthStop(TabStops* pStops, int xLoc, int nth, int xWidth);
int TabStops_GetPreviousNthStop(TabStops* pStops, int xLoc, int nth);


// Character attributes/rendition state
typedef struct CharacterRendition {
    unsigned int    isBold:1;
    unsigned int    isDimmed:1;
    unsigned int    isItalic:1;
    unsigned int    isUnderlined:1;
    unsigned int    isBlink:1;
    unsigned int    isReverse:1;
    unsigned int    isHidden:1;
    unsigned int    isStrikethrough:1;
} CharacterRendition;


// Saved cursor state:
// - cursor position
// - cursor attributes
// - character set
// - origin mode
typedef struct SavedState {
    int                 x;
    int                 y;
    Color               backgroundColor;
    Color               foregroundColor;
    CharacterRendition  characterRendition;
} SavedState;


// Drawing state set up by BeginDrawing() and valid until EndDrawing()
typedef struct GraphicsContext {
    uint8_t* _Nullable  plane[8];
    size_t              bytesPerRow[8];
    size_t              planeCount;
    int                 pixelsWidth;
    int                 pixelsHeight;
} GraphicsContext;


// The console object.
class_ivars(Console, Object,
    Lock                        lock;
    EventDriverRef _Nonnull     eventDriver;
    IOChannelRef _Nonnull       eventDriverChannel;
    GraphicsDriverRef _Nonnull  gdevice;
    const KeyMap* _Nonnull      keyMap;
    vtparser_t                  vtparser;
    RingBuffer                  reportsQueue;
    Color                       backgroundColor;
    Color                       foregroundColor;
    CharacterRendition          characterRendition;
    int                         lineHeight;     // In pixels
    int                         characterWidth; // In pixels
    TabStops                    hTabStops;
    Rect                        bounds;
    int                         x;
    int                         y;
    SavedState                  savedCursorState;
    GraphicsContext             gc;
    SpriteID                    textCursor;
    TimerRef _Nonnull           textCursorBlinker;
    CompatibilityMode           compatibilityMode;
    struct {
        unsigned int    isAutoWrapEnabled: 1;   // true if the cursor should move to the next line if printing a character would move it past teh right margin
        unsigned int    isInsertionMode: 1;     // true if insertion mode is active; false if replace mode is active

        unsigned int    isTextCursorBlinkerEnabled:1;   // true if the text cursor should blink. Visibility is a separate state
        unsigned int    isTextCursorOn:1;               // true if the text cursor blinking state is on; false if off. IsTextCursorVisible has to be true to make the cursor actually visible
        unsigned int    isTextCursorSingleCycleOn:1;    // true if the text cursor should be shown for a single blink cycle even if the cycle is actually supposed to be off. This is set when we print a character to ensure the cursor is visible
        unsigned int    isTextCursorVisible:1;          // global text cursor visibility switch
    }                           flags;
);



extern errno_t Console_InitVideoOutput(ConsoleRef _Nonnull self);
extern void Console_DeinitVideoOutput(ConsoleRef _Nonnull self);

extern void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull pConsole, Color color);
extern void Console_SetBackgroundColor_Locked(ConsoleRef _Nonnull pConsole, Color color);

#define Console_SetDefaultForegroundColor_Locked(__self) \
    Console_SetForegroundColor_Locked(__self, Color_MakeIndex(2)); /* Green */

#define Console_SetDefaultBackgroundColor_Locked(__self) \
    Console_SetBackgroundColor_Locked(__self, Color_MakeIndex(0)); /* Black */

extern void Console_SetCursorBlinkingEnabled_Locked(Console* _Nonnull pConsole, bool isEnabled);
extern void Console_SetCursorVisible_Locked(Console* _Nonnull pConsole, bool isVisible);
extern void Console_OnTextCursorBlink(Console* _Nonnull pConsole);
extern void Console_CursorDidMove_Locked(Console* _Nonnull pConsole);

extern errno_t Console_BeginDrawing_Locked(ConsoleRef _Nonnull self);
extern void Console_EndDrawing_Locked(ConsoleRef _Nonnull self);

extern void Console_DrawGlyph_Locked(ConsoleRef _Nonnull pConsole, char glyph, int x, int y);
extern void Console_CopyRect_Locked(ConsoleRef _Nonnull pConsole, Rect srcRect, Point dstLoc);
extern void Console_FillRect_Locked(ConsoleRef _Nonnull pConsole, Rect rect, char ch);


extern errno_t Console_ResetState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_ResetCharacterAttributes_Locked(ConsoleRef _Nonnull pConsole);

extern void Console_ClearScreen_Locked(Console* _Nonnull pConsole, ClearScreenMode mode);
extern void Console_ClearLine_Locked(ConsoleRef _Nonnull pConsole, int y, ClearLineMode mode);
extern void Console_SaveCursorState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_MoveCursorTo_Locked(Console* _Nonnull pConsole, int x, int y);
extern void Console_MoveCursor_Locked(ConsoleRef _Nonnull pConsole, CursorMovement mode, int dx, int dy);
extern void Console_SetCompatibilityMode_Locked(ConsoleRef _Nonnull pConsole, CompatibilityMode mode);
extern void Console_VT52_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt52parse_action_t action, unsigned char b);
extern void Console_VT100_ParseByte_Locked(ConsoleRef _Nonnull pConsole, vt500parse_action_t action, unsigned char b);

extern void Console_PostReport_Locked(ConsoleRef _Nonnull pConsole, const char* msg);

extern void Console_PrintByte_Locked(ConsoleRef _Nonnull pConsole, unsigned char ch);
extern void Console_Execute_BEL_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_HT_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_LF_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_BS_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_DEL_Locked(ConsoleRef _Nonnull pConsole);
extern void Console_Execute_DCH_Locked(ConsoleRef _Nonnull pConsole, int nChars);
extern void Console_Execute_IL_Locked(ConsoleRef _Nonnull pConsole, int nLines);
extern void Console_Execute_DL_Locked(ConsoleRef _Nonnull pConsole, int nLines);


//
// Keymaps
//
extern const unsigned char gKeyMap_usa[];


//
// Fonts
//
extern const char font8x8_latin1[128][8];
extern const char font8x8_dingbat[160][8];
#define GLYPH_WIDTH     8
#define GLYPH_HEIGHT    8


//
// Text Cursors
//
extern const uint16_t gBlock4x8_Plane0[];
extern const uint16_t gBlock4x8_Plane1[];
extern const int gBlock4x8_Width;
extern const int gBlock4x8_Height;

extern const uint16_t gBlock4x4_Plane0[];
extern const uint16_t gBlock4x4_Plane1[];
extern const int gBlock4x4_Width;
extern const int gBlock4x4_Height;

#endif /* ConsolePriv_h */
