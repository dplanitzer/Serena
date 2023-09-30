//
//  MousePainter.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "MousePainter.h"

// Initializes a new mouse painter. The mouse cursor is by default hidden. Set
// a surface in the painter and then set the cursor visible.
ErrorCode MousePainter_Init(MousePainter* _Nonnull pPainter)
{
    decl_try_err();

    try(kalloc(sizeof(UInt16) * MOUSE_CURSOR_HEIGHT * 2, (Byte**) &pPainter->bitmapMaskBuffer));
    pPainter->bitmap = (UInt16*) pPainter->bitmapMaskBuffer;
    pPainter->mask = (UInt16*) &pPainter->bitmapMaskBuffer[sizeof(UInt16) * MOUSE_CURSOR_HEIGHT];
    pPainter->background = NULL;
    pPainter->rLeft = 0;
    pPainter->rTop = 0;
    pPainter->rRight = 0;
    pPainter->rBottom = 0;

    pPainter->x = 0;
    pPainter->y = 0;
    pPainter->flags.isVisible = false;
    pPainter->flags.isHiddenUntilMouseMoves = false;

    pPainter->curFlags.isVisible = false;
    pPainter->curFlags.hasSavedImage = false;
    pPainter->curX = 0;
    pPainter->curY = 0;
    try(kalloc(sizeof(UInt32) * MOUSE_CURSOR_HEIGHT * 5, (Byte**) &pPainter->savedImage));

    return EOK;

catch:
    return err;
}

void MousePainter_Deinit(MousePainter* _Nonnull pPainter)
{
    kfree((Byte*) pPainter->savedImage);
    pPainter->savedImage = NULL;

    pPainter->background = NULL;
    pPainter->bitmap = NULL;
    pPainter->mask = NULL;

    kfree((Byte*) pPainter->bitmapMaskBuffer);
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

    const Int irs = cpu_disable_irqs();
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
void MousePainter_SetCursor(MousePainter* _Nonnull pPainter, const Byte* pBitmap, const Byte* pMask)
{
    const UInt16* sbp = (const UInt16*)pBitmap;
    const UInt16* smp = (const UInt16*)pMask;
    UInt16* dbp = pPainter->bitmap;
    UInt16* dmp = pPainter->mask;
    Int rows = 0;

    const Int irs = cpu_disable_irqs();
    while (rows++ < MOUSE_CURSOR_HEIGHT) {
        *dbp++ =   *sbp++;
        *dmp++ = ~(*smp++);
    }
    cpu_restore_irqs(irs);
}

void MousePainter_SetPosition(MousePainter* _Nonnull pPainter, Point pt)
{
    const Int irs = cpu_disable_irqs();
    pPainter->x = __min(__max(pt.x, pPainter->rLeft), pPainter->rRight);
    pPainter->y = __min(__max(pt.y, pPainter->rTop), pPainter->rBottom);
    cpu_restore_irqs(irs);
}

Point MousePainter_GetPosition(MousePainter* _Nonnull pPainter)
{
    const Int irs = cpu_disable_irqs();
    const Int x = pPainter->x;
    const Int y = pPainter->y;
    cpu_restore_irqs(irs);

    return Point_Make(x, y);
}

void MousePainter_SetVisible(MousePainter* _Nonnull pPainter, Bool isVisible)
{
    const Int irs = cpu_disable_irqs();
    pPainter->flags.isVisible = isVisible;
    cpu_restore_irqs(irs);
}

void MousePainter_SetHiddenUntilMouseMoves(MousePainter* _Nonnull pPainter, Bool flag)
{
    const Int irs = cpu_disable_irqs();
    // Cursor will be hidden while this flag is true. The vertical blank paint()
    // function will reset it back to false once it detects a move
    pPainter->flags.isHiddenUntilMouseMoves = flag;
    cpu_restore_irqs(irs);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Vertical Blank Interrupt Context
////////////////////////////////////////////////////////////////////////////////

void MousePainter_IncrementPosition_VerticalBlank(MousePainter* _Nonnull pPainter, Int xDelta, Int yDelta)
{
    const Int16 x = pPainter->x + xDelta;
    const Int16 y = pPainter->y += yDelta;

    pPainter->x = __min(__max(x, pPainter->rLeft), pPainter->rRight);
    pPainter->y = __min(__max(y, pPainter->rTop), pPainter->rBottom);
}


static void MousePainter_RestoreSavedImage(MousePainter* _Nonnull pPainter)
{
    Surface* pBackground = pPainter->background;
    register Int8 planeIdx = (Int8) pBackground->planeCount;
    register UInt32 bgBytesPerRow = pBackground->bytesPerRow;
    register const UInt32* sip = pPainter->savedImage;

    while (--planeIdx >= 0) {
        register UInt32* bp = (UInt32*) (pBackground->planes[planeIdx] + pPainter->curSavedByteOffset);
        register Int8 nIters = MOUSE_CURSOR_HEIGHT/4;

        while (nIters-- > 0) {
            *bp = *sip++; bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
            *bp = *sip++; bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
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
    Int8 planeIdx = (Int8) pBackground->planeCount;
    register UInt32 bgBytesPerRow = pBackground->bytesPerRow;
    ByteCount byteOffsetToFirstWord = (pPainter->curY * bgBytesPerRow + (pPainter->curX >> 3)) & ~1;
    register UInt32* sip = pPainter->savedImage;
    UInt8 bitLeft = pPainter->curX - (pPainter->curX >> 4 << 4);    // X coordinate of first bit in the 16bit word we aligned to
    register UInt8 shift = (16 - bitLeft);
    #define rot32_left(bits, shift) ((bits) << (shift)) | ((bits) >> (32ul - (shift)))

    // Plane > 0 -> mouse cursor bits are all 0
    while (--planeIdx > 0) {
        register const UInt16* mp = pPainter->mask;
        register UInt32* bp = (UInt32*) (pBackground->planes[planeIdx] + byteOffsetToFirstWord);
        register Int8 nIters = MOUSE_CURSOR_HEIGHT;

        while (nIters-- > 0) {
            register UInt32 bg = *bp;
            register UInt32 mask = (*mp++ | 0xffff0000ul);

            mask = rot32_left(mask, shift);
            *sip++ = bg;
            *bp = bg & mask;
            bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
        }
    }

    // Plane #0 -> mouse cursor bits are 0 or 1
    register const UInt16* mp = pPainter->mask;
    register const UInt16* mcp = pPainter->bitmap;
    register UInt32* bp = (UInt32*) (pBackground->planes[0] + byteOffsetToFirstWord);
    register Int8 nIters = MOUSE_CURSOR_HEIGHT;

    while (nIters-- > 0) {
        register UInt32 bg = *bp;
        register UInt32 mask = (*mp++ | 0xffff0000ul);
        register UInt32 mouse = (*mcp++) << shift;

        mask = rot32_left(mask, shift);
        *sip++ = bg;
        *bp = (bg & mask) | mouse;
        bp = (UInt32*) ((Byte*)bp + bgBytesPerRow);
    }

    pPainter->curSavedByteOffset = byteOffsetToFirstWord;
    pPainter->curFlags.hasSavedImage = true;
}

void MousePainter_Paint_VerticalBlank(MousePainter* _Nonnull pPainter)
{
    const Bool didMove = (pPainter->curX != pPainter->x) || (pPainter->curY != pPainter->y);

    if (didMove) {
        // A mouse move cancels the hidden-until-mouse-move state
        pPainter->flags.isHiddenUntilMouseMoves = false;
    }

    const Bool isVisibilityRequested = pPainter->flags.isVisible && !pPainter->flags.isHiddenUntilMouseMoves;
    const Bool didVisibilityChange = pPainter->curFlags.isVisible != isVisibilityRequested;
    const Bool hasBackground = pPainter->flags.hasBackground;

    if (pPainter->curFlags.hasSavedImage && (didMove || (didVisibilityChange && !isVisibilityRequested)) && hasBackground) {
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

    if (pPainter->curFlags.isVisible && (didMove || didVisibilityChange) && hasBackground) {
        // Save the image at the current mouse position and the paint the cursor
        // image because:
        // - the mouse was moved (restore of the old image happened above)
        // - a request to show the mouse cursor came in (no restore in this case)
        MousePainter_SaveImageAndPaintCursor(pPainter);
    }
}
