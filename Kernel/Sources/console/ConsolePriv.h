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
#include <dispatchqueue/DispatchQueue.h>
#include <driver/DriverChannel.h>
#include <klib/RingBuffer.h>
#include <sched/mtx.h>
#include "Color.h"
#include "Font.h"
#include "Geometry.h"
#include "KeyMap.h"
#include "TabStops.h"
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
    kCursorMovement_AutoScroll      // Horizontal insertion pointer is clamped and vertical movement will scroll the screen up/down if the insertion pointer moves past the bottom/top edge of the screen
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
    kClearScreenMode_WholeAndScrollback = 3 // Clear the whole screen and the scrollback buffer
} ClearScreenMode;


// Gx character sets
enum CharacterSet {
    kCharacterSet_G0 = 0,
    kCharacterSet_G1 = 1,
    kCharacterSet_G2 = 2,
    kCharacterSet_G3 = 3,
    kCharacterSet_Count = 4
};


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
    int                 gl;     // active character set for GL plane
    int                 x;
    int                 y;
    Color               backgroundColor;
    Color               foregroundColor;
    CharacterRendition  characterRendition;
} SavedState;


// The console object.
final_class_ivars(Console, PseudoDriver,
    mtx_t                       mtx;
    DispatchQueueRef _Nonnull   dispatchQueue;

    vtparser_t                  vtparser;

    IOChannelRef _Nonnull       hidChannel;
    const KeyMap* _Nonnull      keyMap;
    RingBuffer                  reportsQueue;

    GraphicsDriverRef _Nonnull  fb;
    IOChannelRef _Nonnull       fbChannel;
    int                         clutId;
    int                         surfaceId;
    SurfaceMapping              pixels;
    int                         pixelsWidth;
    int                         pixelsHeight;
    int                         textCursorSurface;
    int                         textCursorSprite;
    
    struct timespec             cursorBlinkInterval;
    Color                       backgroundColor;
    Color                       foregroundColor;
    CharacterRendition          characterRendition;
    int                         lineHeight;     // In pixels
    int                         characterWidth; // In pixels
    int                         gl;
    int                         gl_ss23;    // GL saved by SS2/SS3 code (< 0 -> no GL saved; >= 0 GL saved)
    const Font* _Nonnull        g[kCharacterSet_Count];
    TabStops                    hTabStops;
    Rect                        bounds;
    int                         x;
    int                         y;
    SavedState                  savedCursorState;
    CompatibilityMode           compatibilityMode;
    struct {
        unsigned int    isAutoWrapEnabled: 1;   // true if the cursor should move to the next line if printing a character would move it past the right margin
        unsigned int    isInsertionMode: 1;     // true if insertion mode is active; false if replace mode is active

        unsigned int    isTextCursorBlinkerEnabled:1;   // true if the text cursor should blink. Visibility is a separate state
        unsigned int    isTextCursorOn:1;               // true if the text cursor blinking state is on; false if off. IsTextCursorVisible has to be true to make the cursor actually visible
        unsigned int    isTextCursorSingleCycleOn:1;    // true if the text cursor should be shown for a single blink cycle even if the cycle is actually supposed to be off. This is set when we print a character to ensure the cursor is visible
        unsigned int    isTextCursorVisible:1;          // global text cursor visibility switch
    }                           flags;
);



extern errno_t Console_InitVideo(ConsoleRef _Nonnull self);
extern void Console_DeinitVideo(ConsoleRef _Nonnull self);

extern void Console_SetForegroundColor_Locked(ConsoleRef _Nonnull self, Color color);
extern void Console_SetBackgroundColor_Locked(ConsoleRef _Nonnull self, Color color);

#define Console_SetDefaultForegroundColor_Locked(__self) \
    Console_SetForegroundColor_Locked(__self, Color_MakeIndex(2)); /* Green */

#define Console_SetDefaultBackgroundColor_Locked(__self) \
    Console_SetBackgroundColor_Locked(__self, Color_MakeIndex(0)); /* Black */

extern void Console_SetCursorBlinkingEnabled_Locked(ConsoleRef _Nonnull self, bool isEnabled);
extern void Console_SetCursorVisible_Locked(ConsoleRef _Nonnull self, bool isVisible);
extern void Console_OnTextCursorBlink(ConsoleRef _Nonnull self);
extern void Console_CursorDidMove_Locked(ConsoleRef _Nonnull self);

extern void Console_BeginDrawing_Locked(ConsoleRef _Nonnull self);
extern void Console_EndDrawing_Locked(ConsoleRef _Nonnull self);

extern void Console_DrawChar_Locked(ConsoleRef _Nonnull self, char ch, int x, int y);
extern void Console_CopyRect_Locked(ConsoleRef _Nonnull self, Rect srcRect, Point dstLoc);
extern void Console_FillRect_Locked(ConsoleRef _Nonnull self, Rect rect, char ch);


extern errno_t Console_ResetState_Locked(ConsoleRef _Nonnull self, bool shouldStartCursorBlinking);
extern void Console_ResetCharacterAttributes_Locked(ConsoleRef _Nonnull self);

extern void Console_ClearScreen_Locked(ConsoleRef _Nonnull self, ClearScreenMode mode);
extern void Console_ClearLine_Locked(ConsoleRef _Nonnull self, int y, ClearLineMode mode);
extern void Console_SaveCursorState_Locked(ConsoleRef _Nonnull self);
extern void Console_RestoreCursorState_Locked(ConsoleRef _Nonnull self);
extern void Console_MoveCursorTo_Locked(ConsoleRef _Nonnull self, int x, int y);
extern void Console_MoveCursor_Locked(ConsoleRef _Nonnull self, CursorMovement mode, int dx, int dy);
extern void Console_SetCompatibilityMode_Locked(ConsoleRef _Nonnull self, CompatibilityMode mode);
extern void Console_VT52_ParseByte_Locked(ConsoleRef _Nonnull self, vt52parse_action_t action, unsigned char b);
extern void Console_VT100_ParseByte_Locked(ConsoleRef _Nonnull self, vt500parse_action_t action, unsigned char b);

extern void Console_PostReport_Locked(ConsoleRef _Nonnull self, const char* msg);

extern void Console_PrintByte_Locked(ConsoleRef _Nonnull self, unsigned char ch);
extern void Console_Execute_BEL_Locked(ConsoleRef _Nonnull self);
extern void Console_Execute_HT_Locked(ConsoleRef _Nonnull self);
extern void Console_Execute_LF_Locked(ConsoleRef _Nonnull self);
extern void Console_Execute_BS_Locked(ConsoleRef _Nonnull self);
extern void Console_Execute_DEL_Locked(ConsoleRef _Nonnull self);
extern void Console_Execute_DCH_Locked(ConsoleRef _Nonnull self, int nChars);
extern void Console_Execute_IL_Locked(ConsoleRef _Nonnull self, int nLines);
extern void Console_Execute_DL_Locked(ConsoleRef _Nonnull self, int nLines);


//
// Console Channel
//

// Big enough to hold the result of a key mapping and the longest possible
// terminal report message.
#define MAX_MESSAGE_LENGTH kKeyMap_MaxByteSequenceLength

// The console I/O channel
//
// Takes care of mapping a USB key scan code to a character or character sequence.
// We may leave partial character sequences in the buffer if a Console_Read() didn't
// read all bytes of a sequence. The next Console_Read() will first receive the
// remaining buffered bytes before it receives bytes from new events.
typedef struct ConsoleChannel{
    char    rdBuffer[MAX_MESSAGE_LENGTH];   // Holds a full or partial byte sequence produced by a key down event
    int8_t  rdCount;                        // Number of bytes stored in the buffer
    int8_t  rdIndex;                        // Index of first byte in the buffer where a partial byte sequence begins
} ConsoleChannel;


//
// Keymaps
//
extern const unsigned char gKeyMap_usa[];


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
