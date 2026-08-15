//
//  gd/cmds.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_CMDS_H
#define _GD_CMDS_H 1

#include <stdbool.h>
#include <stdint.h>
#include <gd/types.h>

//
// Universal Commands
//

extern void gdCmdBegin(gd_cmdbuf_t _Nonnull cmdbuf);
extern void gdCmdEnd(gd_cmdbuf_t _Nonnull cmdbuf);


//
// Transfer Queue Commands
//

extern void gdCmdWritePixels(gd_cmdbuf_t _Nonnull cmdbuf, gd_image_t _Nonnull img, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);
extern void gdCmdClearPixels(gd_cmdbuf_t _Nonnull cmdbuf, gd_image_t _Nonnull img);


//
// Sprite Queue Commands
//

extern void gdCmdBindSpriteImage(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, gd_image_t _Nonnull img);
extern void gdCmdMoveSprite(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, int16_t x, int16_t y);
extern void gdCmdShowSprite(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, bool isVisible);

#endif /* _GD_CMDS_H */
