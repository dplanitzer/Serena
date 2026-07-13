//
//  gd_cmdbuf.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include "video_conf.h"
#include <ext/math.h>
#include <ext/queue.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>

typedef struct cmdbuf {
    deque_node_t            chain;
    int                     id;
    size_t                  byteSize;
    const char* _Nonnull    op;
    const char* _Nonnull    opEnd;
} cmdbuf_t;


static int      g_next_cmdbuf_id = 1;
static deque_t  g_cmdbuf_list;



static cmdbuf_t* _Nullable _cmdbuf_for_id(int id)
{
    deque_for_each(&g_cmdbuf_list, struct cmdbuf, it,
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}



errno_t gdGenCmdbuf(size_t reqSize, vio_cmdbuf_desc_t* _Nonnull desc)
{
    decl_try_err();
    cmdbuf_t* cmdbuf;
    void* opbuf;

    if (reqSize < sizeof(vio_opcode_t) || reqSize > SIZE_KB(8)) {
        return EINVAL;
    }

    err = kalloc_cleared(sizeof(struct cmdbuf), (void**)&cmdbuf);
    if (err != EOK) {
        return err;
    }

    err = kalloc_cleared(reqSize, &opbuf);
    if (err != EOK) {
        kfree(cmdbuf);
        return err;
    }


    cmdbuf->id = g_next_cmdbuf_id++;
    cmdbuf->byteSize = reqSize;
    cmdbuf->op = opbuf;
    cmdbuf->opEnd =  cmdbuf->op + cmdbuf->byteSize;

    desc->addr = (void*)cmdbuf->op;
    desc->size = cmdbuf->byteSize;
    desc->id = cmdbuf->id;

    deque_add_first(&g_cmdbuf_list, &cmdbuf->chain);

    return EOK;
}

errno_t gdDeleteCmdbuf(int id)
{
    if (id == 0) {
        return EOK;
    }

    cmdbuf_t* cmdbuf = _cmdbuf_for_id(id);
    if (cmdbuf == NULL) {
        return EINVAL;
    }

    kfree(cmdbuf->op);
    kfree(cmdbuf);

    return EOK;
}

errno_t gdExecCmdbuf(int id, size_t offset)
{
    decl_try_err();
    cmdbuf_t* cmdbuf = _cmdbuf_for_id(id);

    if (cmdbuf == NULL) {
        return EINVAL;
    }


    const union vio_op* ip = (const union vio_op*)(cmdbuf->op + offset);
    size_t ilen;

    while ((const char*)ip < cmdbuf->opEnd) {
        switch (ip->opcode) {
            case VIO_OPCODE_NOP:                // vio_opcode_t
                ilen = 0;
                break;

            case VIO_OPCODE_END:                // vio_opcode_t
                return EOK;

            case VIO_OPCODE_WRITE_PIXELS:       // struct vio_op_write_pixels
                try(gdWritePixels(ip->write_pixels.bufferId, &ip->write_pixels.plane[0], ip->write_pixels.bytesPerRow, ip->write_pixels.format));
                ilen = sizeof(struct vio_op_write_pixels) + (PixelFormat_GetPlaneCount(ip->write_pixels.format) - 1) * sizeof(void*);
                break;

            case VIO_OPCODE_CLEAR_PIXELS:       // struct vio_op_buffer
                try(gdClearPixels(ip->buffer.bufferId));
                ilen = sizeof(struct vio_op_buffer);
                break;

            case VIO_OPCODE_BIND_BUFFER:        // vio_op_bind_buffer
                try(gdBindBuffer(ip->bind_buffer.target, ip->bind_buffer.bufferId));
                ilen = sizeof(struct vio_op_bind_buffer);
                break;
                
            case VIO_OPCODE_CLUT_RGB32:         // struct vio_op_clut_rgb32
                try(gdSetClutEntries(ip->clut_rgb32.clutId, ip->clut_rgb32.idx, ip->clut_rgb32.count, &ip->clut_rgb32.color[0]));
                ilen = sizeof(struct vio_op_clut_rgb32) - sizeof(vio_rgb32_t);
                if (ip->clut_rgb32.count > 0) {
                    ilen += sizeof(vio_rgb32_t) * ip->clut_rgb32.count;
                }
                break;

            case VIO_OPCODE_PUT_SPRITE:         // struct vio_op_put_sprite
                try(gdSetSpritePos(ip->put_sprite.spriteId, ip->put_sprite.x, ip->put_sprite.y));
                ilen = sizeof(struct vio_op_put_sprite);
                break;

            case VIO_OPCODE_SHOW_SPRITE:        // struct vio_op_show_sprite
                try(gdSetSpriteVis(ip->show_sprite.spriteId, ip->show_sprite.visible != 0));
                ilen = sizeof(struct vio_op_show_sprite);
                break;

            default:
                return EINVAL;
        }

        ip = (const union vio_op*)((const char*)ip + ilen);
    }

catch:
    return err;
}
