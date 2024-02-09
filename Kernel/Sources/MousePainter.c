//
//  MousePainter.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "MousePainter.h"

// XXX
// TODO:
// - Unshield() leaves a copy of the mouse cursor behind if the mouse cursor is outside the shielding rect
// - we need to clip the mouse cursor if it goes partially (completely) off screen
// - SetMouseCursor() does not immediately update the screen. You have to move the mouse to see the change
// - SetVisible() may have the same problem. Need to check
// XXX
static void MousePainter_RestoreSavedImage(MousePainter* _Nonnull pPainter);
static void MousePainter_SaveImageAndPaintCursor(MousePainter* _Nonnull pPainter);


// Initializes a new mouse painter. The mouse cursor is by default hidden. Set
// a surface in the painter and then set the cursor visible.
errno_t MousePainter_Init(MousePainter* _Nonnull pPainter)
{
    decl_try_err();

    try(kalloc(sizeof(uint16_t) * MOUSE_CURSOR_HEIGHT * 2, (void**) &pPainter->bitmapMaskBuffer));
    pPainter->bitmap = (uint16_t*) pPainter->bitmapMaskBuffer;
    pPainter->mask = (uint16_t*) &pPainter->bitmapMaskBuffer[sizeof(uint16_t) * MOUSE_CURSOR_HEIGHT];
    pPainter->background = NULL;
    pPainter->rLeft = 0;
    pPainter->rTop = 0;
    pPainter->rRight = 0;
    pPainter->rBottom = 0;

    pPainter->x = 0;
    pPainter->y = 0;
    pPainter->flags.isVisible = false;
    pPainter->flags.isHiddenUntilMouseMoves = false;
    pPainter->flags.hasBackground = false;

    pPainter->curFlags.isVisible = false;
    pPainter->curFlags.isShielded = false;
    pPainter->curFlags.hasSavedImage = false;
    pPainter->curX = 0;
    pPainter->curY = 0;
    try(kalloc(sizeof(uint32_t) * MOUSE_CURSOR_HEIGHT * 5, (void**) &pPainter->savedImage));

    return EOK;

catch:
    return err;
}

void MousePainter_Deinit(MousePainter* _Nonnull pPainter)
{
    kfree(pPainter->savedImage);
    pPainter->savedImage = NULL;

    pPainter->background = NULL;
    pPainter->bitmap = NULL;
    pPainter->mask = NULL;

    kfree(pPainter->bitmapMaskBuffer);
    pPainter->bitmapMaskBuffer = NULL;
}

// Sets the surface that holds the background pixels over which the mouse cursor
// should hover. This is typically the framebuffer. Note that the painter holds
// a weak reference to the surface. Expects that the size of the surface is at
// least 32 by 16 pixels. Expects that the pixels of the surface are locked for
// reading and writing and stay locked as long as the surface is attached to the
// painter.
// Note that setting a new surface on the mouse painter implicitly hides the
// mouse cursor and cancels the hide-until-mouse-moved state. You have to
// explicitly turn the mouse cursor back on if so desired.
void MousePainter_SetSurface(MousePainter* _Nonnull pPainter, Surface* _Nullable pSurface)
{
    const Size sSize = (pSurface) ? Surface_GetPixelSize(pSurface) : Size_Zero;

    const int irs = cpu_disable_irqs();
    pPainter->background = pSurface;
    if (pSurface) {
        assert(sSize.width >= 32 && sSize.height >= 16);
        pPainter->rLeft = 0;
        pPainter->rTop = 0;
        pPainter->rRight = pPainter->rLeft + sSize.width;
        pPainter->rBottom = pPainter->rTop + sSize.height;
        pPainter->flags.hasBackground = true;
    } else {
        pPainter->rLeft = 0;
        pPainter->rTop = 0;
        pPainter->rRight = 0;
        pPainter->rBottom = 0;
        pPainter->flags.hasBackground = false;
    }

    pPainter->x = __min(__max(pPainter->x, pPainter->rLeft), pPainter->rRight);
    pPainter->y = __min(__max(pPainter->y, pPainter->rTop), pPainter->rBottom);
    pPainter->flags.isHiddenUntilMouseMoves = false;
    pPainter->flags.isVisible = false;

    // Make sure that the paint() function doesn't do anything until the caller
    // turns the mouse cursor back on since the surface has changed and we don't
    // want any spurious painting to happen.
    pPainter->curFlags.isVisible = false;
    pPainter->curFlags.isShielded = false;
    pPainter->curFlags.hasSavedImage = false;
    pPainter->curX = pPainter->x;
    pPainter->curY = pPainter->y;
    cpu_restore_irqs(irs);
}

// Sets the mouse cursor image and mask. The bitmap must be 16 pixels wide and
// 16 rows high. The mask must be of the same size. The origin of the bitmap and
// mask is in the top-left corner (0, 0). One bit represents one pixel where a
// 0 bit is the mouse cursor background color and a 1 bit is the mouse cursor
// foreground color. The mouse cursor image will only appear in places where the
// mask stores a 1 bit. The framebuffer image will appear in places where the
// mask stores a 0 bit. Assumes that the bytes-per-row value of the bitmap and
// mask are 2.
void MousePainter_SetCursor(MousePainter* _Nonnull pPainter, const void* pBitmap, const void* pMask)
{
    const uint16_t* sbp = (const uint16_t*)pBitmap;
    const uint16_t* smp = (const uint16_t*)pMask;
    uint16_t* dbp = pPainter->bitmap;
    uint16_t* dmp = pPainter->mask;
    int rows = 0;

    const int irs = cpu_disable_irqs();
    while (rows++ < MOUSE_CURSOR_HEIGHT) {
        *dbp++ =   *sbp++;
        *dmp++ = ~(*smp++);
    }
    cpu_restore_irqs(irs);
}

void MousePainter_SetPosition(MousePainter* _Nonnull pPainter, Point pt)
{
    const int irs = cpu_disable_irqs();
    pPainter->x = __min(__max(pt.x, pPainter->rLeft), pPainter->rRight);
    pPainter->y = __min(__max(pt.y, pPainter->rTop), pPainter->rBottom);
    cpu_restore_irqs(irs);
}

Point MousePainter_GetPosition(MousePainter* _Nonnull pPainter)
{
    const int irs = cpu_disable_irqs();
    const int x = pPainter->x;
    const int y = pPainter->y;
    cpu_restore_irqs(irs);

    return Point_Make(x, y);
}

void MousePainter_SetVisible(MousePainter* _Nonnull pPainter, bool isVisible)
{
    const int irs = cpu_disable_irqs();
    pPainter->flags.isVisible = isVisible;
    cpu_restore_irqs(irs);
}

void MousePainter_SetHiddenUntilMouseMoves(MousePainter* _Nonnull pPainter, bool flag)
{
    const int irs = cpu_disable_irqs();
    // Cursor will be hidden while this flag is true. The vertical blank paint()
    // function will reset it back to false once it detects a move
    pPainter->flags.isHiddenUntilMouseMoves = flag;
    cpu_restore_irqs(irs);
}

// Shields the mouse cursor if it intersects the given rectangle. Shielding means
// that (a) the mouse cursor is immediately and synchronously hidden (rather than
// asynchronously by waiting until the next vertical blank interrupt) and (b) the
// mouse cursor stays hidden until it is unshielded. These two functions should
// be used by drawing routines that draw into the framebuffer to ensure that their
// drawing doesn't get mixed up incorrectly with the mouse cursor image.
void MousePainter_ShieldCursor(MousePainter* _Nonnull pPainter, const Rect r)
{
    // XXX analyze to find out whether we can avoid turning IRQs off here for the
    // XXX common case that the mouse cursor doesn't intersect 'r'
    const int irs = cpu_disable_irqs();

    if (!pPainter->curFlags.isShielded) {
        pPainter->curFlags.isShielded = true;

        if (pPainter->curFlags.hasSavedImage && pPainter->flags.hasBackground) {
            const Rect crsrRect = Rect_Make(pPainter->curX, pPainter->curY,
                pPainter->x + MOUSE_CURSOR_WIDTH, pPainter->y + MOUSE_CURSOR_HEIGHT);

            if (Rect_IntersectsRect(crsrRect, r)) {
                MousePainter_RestoreSavedImage(pPainter);
            }
        }
    }

    cpu_restore_irqs(irs);
}

void MousePainter_UnshieldCursor(MousePainter* _Nonnull pPainter)
{
    // XXX analyze whether we can do things here without always turning the IRQs off
    const int irs = cpu_disable_irqs();

    if (pPainter->curFlags.isShielded) {
        if (pPainter->curFlags.isVisible && pPainter->flags.hasBackground) {
            MousePainter_SaveImageAndPaintCursor(pPainter);
        }

        pPainter->curFlags.isShielded = false;
    }

    cpu_restore_irqs(irs);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Vertical Blank Interrupt Context
////////////////////////////////////////////////////////////////////////////////

void MousePainter_SetPosition_VerticalBlank(MousePainter* _Nonnull pPainter, int16_t x, int16_t y)
{
    pPainter->x = __min(__max(x, pPainter->rLeft), pPainter->rRight);
    pPainter->y = __min(__max(y, pPainter->rTop), pPainter->rBottom);
}


static void MousePainter_RestoreSavedImage(MousePainter* _Nonnull pPainter)
{
    Surface* pBackground = pPainter->background;
    register int8_t planeIdx = (int8_t) pBackground->planeCount;
    register uint32_t bgBytesPerRow = pBackground->bytesPerRow;
    register const uint32_t* sip = pPainter->savedImage;

    while (--planeIdx >= 0) {
        register uint32_t* bp = (uint32_t*) (pBackground->planes[planeIdx] + pPainter->curSavedByteOffset);
        register int8_t nIters = MOUSE_CURSOR_HEIGHT/4;

        while (nIters-- > 0) {
            *bp = *sip++; bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
        }
    }
    pPainter->curFlags.hasSavedImage = false;
}

static void MousePainter_SaveImageAndPaintCursor(MousePainter* _Nonnull pPainter)
{
    // Calculate the byte offset to the long word whose bit #31 corresponds to
    // the top-left corner of the image and store it along with the image.

    // Save the image to our buffer
    Surface* pBackground = pPainter->background;
    int8_t planeIdx = (int8_t) pBackground->planeCount;
    register uint32_t bgBytesPerRow = pBackground->bytesPerRow;
    ssize_t byteOffsetToFirstWord = (pPainter->curY * bgBytesPerRow + (pPainter->curX >> 3)) & ~1;
    register uint32_t* sip = pPainter->savedImage;
    uint8_t bitLeft = pPainter->curX - (pPainter->curX >> 4 << 4);    // X coordinate of first bit in the 16bit word we aligned to
    register uint8_t shift = (16 - bitLeft);
    #define rot32_left(bits, shift) ((bits) << (shift)) | ((bits) >> (32ul - (shift)))

    // Plane > 0 -> mouse cursor bits are all 0
    while (--planeIdx > 0) {
        register const uint16_t* mp = pPainter->mask;
        register uint32_t* bp = (uint32_t*) (pBackground->planes[planeIdx] + byteOffsetToFirstWord);
        register int8_t nIters = MOUSE_CURSOR_HEIGHT;

        while (nIters-- > 0) {
            register uint32_t bg = *bp;
            register uint32_t mask = (*mp++ | 0xffff0000ul);

            mask = rot32_left(mask, shift);
            *sip++ = bg;
            *bp = bg & mask;
            bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
        }
    }

    // Plane #0 -> mouse cursor bits are 0 or 1
    register const uint16_t* mp = pPainter->mask;
    register const uint16_t* mcp = pPainter->bitmap;
    register uint32_t* bp = (uint32_t*) (pBackground->planes[0] + byteOffsetToFirstWord);
    register int8_t nIters = MOUSE_CURSOR_HEIGHT;

    while (nIters-- > 0) {
        register uint32_t bg = *bp;
        register uint32_t mask = (*mp++ | 0xffff0000ul);
        register uint32_t mouse = (*mcp++) << shift;

        mask = rot32_left(mask, shift);
        *sip++ = bg;
        *bp = (bg & mask) | mouse;
        bp = (uint32_t*) ((uint8_t*)bp + bgBytesPerRow);
    }

    pPainter->curSavedByteOffset = byteOffsetToFirstWord;
    pPainter->curFlags.hasSavedImage = true;
}

void MousePainter_Paint_VerticalBlank(MousePainter* _Nonnull pPainter)
{
    const bool didMove = (pPainter->curX != pPainter->x) || (pPainter->curY != pPainter->y);

    if (didMove) {
        // A mouse move cancels the hidden-until-mouse-move state
        pPainter->flags.isHiddenUntilMouseMoves = false;
    }

    const bool isVisibilityRequested = pPainter->flags.isVisible && !pPainter->flags.isHiddenUntilMouseMoves;
    const bool didVisibilityChange = pPainter->curFlags.isVisible != isVisibilityRequested;
    const bool hasBackground = pPainter->flags.hasBackground;

    if (pPainter->curFlags.hasSavedImage && (didMove || (didVisibilityChange && !isVisibilityRequested)) && hasBackground && !pPainter->curFlags.isShielded) {
        // Restore the saved image because we are currently visible and:
        // - the mouse has moved
        // - we've received a request to hide the mouse because either
        //   flags.isVisible is false or flags.isHiddenUntilMouseMoves is true
        //   and no movement has happened
        MousePainter_RestoreSavedImage(pPainter);
    }

    pPainter->curX = pPainter->x;
    pPainter->curY = pPainter->y;
    pPainter->curFlags.isVisible = isVisibilityRequested;

    if (pPainter->curFlags.isVisible && (didMove || didVisibilityChange) && hasBackground && !pPainter->curFlags.isShielded) {
        // Save the image at the current mouse position and the paint the cursor
        // image because:
        // - the mouse was moved (restore of the old image happened above)
        // - a request to show the mouse cursor came in (no restore in this case)
        MousePainter_SaveImageAndPaintCursor(pPainter);
    }
}
