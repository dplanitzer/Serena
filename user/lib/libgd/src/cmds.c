//
//  cmds.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <gd/cmds.h>
#include <stdlib.h>


//XXX
// Should add something similar to Vulkan command validation where we check for
// command buffer overflow, whether arguments are valid, etc. 
//XXX

static int8_t _gdGetPlaneCount(gd_pixfmt_t format)
{
    switch (format) {
        case GD_RGB_SPRITE_2:
        case GD_COLOR_INDEX1:
            return 1;

        case GD_COLOR_INDEX2:
            return 2;

        case GD_COLOR_INDEX3:
            return 3;

        case GD_COLOR_INDEX4:
            return 4;

        case GD_COLOR_INDEX5:
            return 5;

        case GD_RGB_HAM_6:
        case GD_COLOR_INDEX6:
            return 6;

        case GD_COLOR_INDEX7:
            return 7;

        case GD_COLOR_INDEX8:
            return 8;
            
        default:
            return 1;
    }
}


//
// Universal Commands
//

void gdCmdBegin(gd_cmdbuf_t _Nonnull cmdbuf)
{
    struct gd_cmdbuf* cbp = cmdbuf;

    cbp->next_addr = cbp->base_addr;
}

void gdCmdEnd(gd_cmdbuf_t _Nonnull cmdbuf)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    gd_opcode_t* p = (gd_opcode_t*)cbp->next_addr;

    *p = GD_OPCODE_END;
    cbp->next_addr += sizeof(gd_opcode_t);
}


//
// Transfer Queue Commands
//

void gdCmdWritePixels(gd_cmdbuf_t _Nonnull cmdbuf, gd_image_t _Nonnull img, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    struct gd_image* ip = img;
    struct gd_op_write_pixels* p = (struct gd_op_write_pixels*)cbp->next_addr;
    const size_t pcnt = _gdGetPlaneCount(format);

    p->opcode = GD_OPCODE_WRITE_PIXELS;
    p->dstImageId = ip->img_d;
    p->bytesPerRow = bytesPerRow;
    p->format = format;
    
    for (size_t i = 0; i < pcnt; i++) {
        p->plane[i] = planes[i];
    }

    cbp->next_addr += sizeof(struct gd_op_write_pixels) + (pcnt - 1) * sizeof(void*);
}

void gdCmdClearPixels(gd_cmdbuf_t _Nonnull cmdbuf, gd_image_t _Nonnull img)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    struct gd_image* ip = img;
    struct gd_op_clear_pixels* p = (struct gd_op_clear_pixels*)cbp->next_addr;

    p->opcode = GD_OPCODE_CLEAR_PIXELS;
    p->dstImageId = ip->img_d;

    cbp->next_addr += sizeof(struct gd_op_clear_pixels);
}


//
// Sprite Queue Commands
//

void gdCmdBindSpriteImage(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, gd_image_t _Nonnull img)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    struct gd_image* ip = img;
    struct gd_sprite* sp = spr;
    struct gd_op_sprite_image* p = (struct gd_op_sprite_image*)cbp->next_addr;

    p->opcode = GD_OPCODE_SPRITE_IMAGE;
    p->spriteId = sp->sprite_d;
    p->imageId = ip->img_d;

    cbp->next_addr += sizeof(struct gd_op_sprite_image);
}

void gdCmdMoveSprite(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, int16_t x, int16_t y)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    struct gd_sprite* sp = spr;
    struct gd_op_sprite_move* p = (struct gd_op_sprite_move*)cbp->next_addr;

    p->opcode = GD_OPCODE_SPRITE_MOVE;
    p->spriteId = sp->sprite_d;
    p->x = x;
    p->y = y;

    cbp->next_addr += sizeof(struct gd_op_sprite_move);
}

void gdCmdShowSprite(gd_cmdbuf_t _Nonnull cmdbuf, gd_sprite_t _Nonnull spr, bool isVisible)
{
    struct gd_cmdbuf* cbp = cmdbuf;
    struct gd_sprite* sp = spr;
    struct gd_op_sprite_show* p = (struct gd_op_sprite_show*)cbp->next_addr;

    p->opcode = GD_OPCODE_SPRITE_SHOW;
    p->spriteId = sp->sprite_d;
    p->visible = isVisible;
    
    cbp->next_addr += sizeof(struct gd_op_sprite_show);
}
