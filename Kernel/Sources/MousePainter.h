//
//  MousePainter.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef MousePainter_h
#define MousePainter_h

#include <klib/klib.h>
#include "Surface.h"

// Currently hard limits for a mouse image and mask. Both must be of this size.
#define MOUSE_CURSOR_WIDTH  16
#define MOUSE_CURSOR_HEIGHT 16

typedef struct _MousePainter {
    uint16_t* _Nonnull _Weak      bitmap;
    uint16_t* _Nonnull _Weak      mask;               // Mask is inverted: 0 where the mouse image should appear; 1 where the background should appear
    Byte* _Nonnull              bitmapMaskBuffer;   // Common storage for the bitmap and mask. Stored in non-unified RAM
    Surface* _Nullable _Weak    background;         // The background image over which the cursor should hover
    int16_t                       rLeft;              // Screen bounds in top-left, bottom-right notation. Note that the bottom-right coordinate is exclusive
    int16_t                       rTop;
    int16_t                       rRight;
    int16_t                       rBottom;

    // Requested mouse cursor position and visibility. The paint function is
    // responsible for figuring out the difference between what the requested
    // state is and what the current state is and it then reconciles the states
    // by updated the current state to reflect the requested state. Note that
    // there may be many (redundant) changes to the requested state over the
    // course of a single frame. Eg the mouse cursor may be turned off and on
    // again multiple times. The only state difference that matters in the end
    // is the requested state when the next vertical blank happens. This one will
    // be compared to the current state of the mouse cursor as it appears on the
    // screen. 
    int16_t                       x;
    int16_t                       y;
    struct _Flags {
        uint8_t                   isVisible:1;
        uint8_t                   isHiddenUntilMouseMoves:1;
        uint8_t                   hasBackground:1;
    }                           flags;

    // Current mouse cursor state corresponding to what is visible on the screen right now
    struct _CurrentFlags {
        uint8_t                   isVisible:1;
        uint8_t                   isShielded:1;
        uint8_t                   hasSavedImage:1;
    }                           curFlags;
    int16_t                       curX;
    int16_t                       curY;
    ssize_t                   curSavedByteOffset; // Offset from the top-left corner of the framebuffer to the top-left corner of the saved image. This is in terms of bytes
    uint32_t*                     savedImage;         // Buffer is big enough to hold a 32x16 image with 5 bitplanes. This is 32bit because the 16bit (pixel) wide mouse cursor may straddle two adjacend 16bit words
                                                    // Planes are stored one after the other without any padding bytes in between. The order is plane n-1, n-2, ..., 0
} MousePainter;


// Initializes a new mouse painter. The mouse cursor is by default hidden. Set
// a surface in the painter and then set the cursor visible.
extern errno_t MousePainter_Init(MousePainter* _Nonnull pPainter);
extern void MousePainter_Deinit(MousePainter* _Nonnull pPainter);

// Sets the surface that holds the background pixels over which the mouse cursor
// should hover. This is typically the framebuffer. Note that the painter holds
// a weak reference to the surface. Expects that the size of the surface is at
// least 32 by 16 pixels. Expects that the pixels of the surface are locked for
// reading and writing and stay locked as long as the surface is attached to the
// painter.
// Note that setting a new surface on the mouse painter implicitly hides the
// mouse cursor and cancels the hide-until-mouse-moved state. You have to
// explicitly turn the mouse cursor back on if so desired.
extern void MousePainter_SetSurface(MousePainter* _Nonnull pPainter, Surface* _Nullable pSurface);

// Sets the mouse cursor image and mask. The bitmap must be 16 pixels wide and
// 16 rows high. The mask must be of the same size. The origin of the bitmap and
// mask is in the top-left corner (0, 0). One bit represents one pixel where a
// 0 bit is the mouse cursor background color and a 1 bit is the mouse cursor
// foreground color. The mouse cursor image will only appear in places where the
// mask stores a 1 bit. The framebuffer image will appear in places where the
// mask stores a 0 bit. Assumes that the bytes-per-row value of the bitmap and
// mask are 2.
extern void MousePainter_SetCursor(MousePainter* _Nonnull pPainter, const Byte* pBitmap, const Byte* pMask);
extern void MousePainter_SetPosition(MousePainter* _Nonnull pPainter, Point pt);
extern void MousePainter_SetVisible(MousePainter* _Nonnull pPainter, bool isVisible);
extern void MousePainter_SetHiddenUntilMouseMoves(MousePainter* _Nonnull pPainter, bool flag);

extern Point MousePainter_GetPosition(MousePainter* _Nonnull pPainter);

// Shields the mouse cursor if it intersects the given rectangle. Shielding means
// that (a) the mouse cursor is immediately and synchronously hidden (rather than
// asynchronously by waiting until the next vertical blank interrupt) and (b) the
// mouse cursor stays hidden until it is unshielded. These two functions should
// be used by drawing routines that draw into the framebuffer to ensure that their
// drawing doesn't get mixed up incorrectly with the mouse cursor image.
extern void MousePainter_ShieldCursor(MousePainter* _Nonnull pPainter, const Rect r);
extern void MousePainter_UnshieldCursor(MousePainter* _Nonnull pPainter);


// The following APIs are designed to be called from the vertical blank interrupt
// context

extern void MousePainter_SetPosition_VerticalBlank(MousePainter* _Nonnull pPainter, int16_t x, int16_t y);
extern void MousePainter_Paint_VerticalBlank(MousePainter* _Nonnull pPainter);

#endif /* MousePainter_h */
