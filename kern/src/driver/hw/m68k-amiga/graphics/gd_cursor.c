//
//  gd_cursor.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/6/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <ext/math.h>
#include <kpi/process.h>

#define MOUSE_SPRITE_PRI 0


static int16_t              g_hot_spot_x;
static int16_t              g_hot_spot_y;
static image_t* _Nonnull    g_cursor_image;
static int                  g_cursor_sprite_id;

errno_t _gdInitCursor(void)
{
    g_cursor_sprite_id = -1;
    return _gdCreateImage(PID_KERNELD, HID_CURSOR_WIDTH, HID_CURSOR_HEIGHT, GD_RGB_SPRITE_2, &g_cursor_image);
}

errno_t gdSetCursor(const IOHIDCursor* _Nullable cursor)
{
    decl_try_err();
    bool doRelease = false;

    if (cursor) {
        if (cursor->type != HID_STRUCTURE_TYPE_CURSOR) {
            return EINVAL;
        }
        if ((cursor->planes && cursor->bytesPerRow == 0) || cursor->width != HID_CURSOR_WIDTH || cursor->height != HID_CURSOR_HEIGHT || cursor->pixelFormat != HID_CURSOR_PIXELFORMAT) {
            return EINVAL;
        }
        if (cursor->hotSpotX < 0 || cursor->hotSpotX >= cursor->width || cursor->hotSpotY < 0 || cursor->hotSpotY >= cursor->height) {
            return EINVAL;
        }
    }


    if (cursor) {
        if (g_cursor_sprite_id < 0) {
            // Acquire the mouse cursor sprite, since we previously didn't have it
            err = gdAcquireSprites(PID_KERNELD, MOUSE_SPRITE_PRI, 1, &g_cursor_sprite_id);
            if (err == EOK) {
                err = _gdBindSpriteImage(PID_KERNELD, g_cursor_sprite_id, g_cursor_image);
                if (err != EOK) {
                    doRelease = true;
                }
            }
        }


        if (err == EOK) {
            // Update the mouse cursor image and hot spot
            _gdWritePixels(g_cursor_image, cursor->planes, cursor->bytesPerRow, cursor->pixelFormat);

            if (g_hot_spot_x != cursor->hotSpotX || g_hot_spot_y != cursor->hotSpotY) {
                const int16_t newX = g_sprite[MOUSE_SPRITE_PRI].x + g_hot_spot_x;   // gdUpdateCursor() does the (x) - cursor->hotSpot term
                const int16_t newY = g_sprite[MOUSE_SPRITE_PRI].y + g_hot_spot_y;

                g_hot_spot_x = cursor->hotSpotX;
                g_hot_spot_y = cursor->hotSpotY;

                gdUpdateCursor(newX, newY, IOHID_CURSOR_CHANGE_POSITION);
            }
        }
    }
    else if (g_cursor_sprite_id >= 0) {
        // 'cursor' is NULL and we got a mouse cursor -> free the sprite
        // (but keep the cursor image memory around for future requests)
        doRelease = true;
    }


    if (doRelease) {
        gdReleaseSprites(PID_KERNELD, &g_cursor_sprite_id, 1);
        g_cursor_sprite_id = -1;
        g_hot_spot_x = 0;
        g_hot_spot_y = 0;
    }

    return err;
}

void gdUpdateCursor(int16_t x, int16_t y, unsigned int flags)
{
    sprite_channel_t* scp = &g_sprite[MOUSE_SPRITE_PRI];

    if (scp->ownerPid != PID_KERNELD) {
        return;
    }


    if ((flags & IOHID_CURSOR_CHANGE_VISIBILITY) != 0) {
        const bool isVisible = (flags & IOHID_CURSOR_VISIBLE) ? true : false;

        if (scp->isVisible != isVisible) {
            scp->isVisible = isVisible;

            if (scp->image) {
                _sprite_image_or_visibility_changed(scp);
            }
        }
    }


    if ((flags & IOHID_CURSOR_CHANGE_POSITION) != 0) {
        scp->x = x - g_hot_spot_x;
        scp->y = y - g_hot_spot_y;

        if (scp->image) {
            const uint32_t ctl = _calc_sprite_ctl(scp);
            uint32_t* ctl_ptr = (uint32_t*)_gdGetImagePlane(scp->image, 0);

            if (scp->isVisible) {
                sprite_ctl_submit(scp->id, ctl_ptr, ctl);
            }
            else {
                *ctl_ptr = ctl;
            }
        }
    }
}
