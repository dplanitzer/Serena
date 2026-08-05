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
    pid_t                   ownerPid;
    size_t                  byteSize;
    const char* _Nonnull    op;
    const char* _Nonnull    opEnd;
} cmdbuf_t;


static int      g_next_cmdimg_id = 1;
static deque_t  g_cmdbuf_list;



static cmdbuf_t* _Nullable _cmdbuf_for_id(pid_t pid, int id)
{
    deque_for_each(&g_cmdbuf_list, struct cmdbuf, it,
        //XXX can not currently do the check for 'ownerPid == pid' because the
        // console is still inside the kernel and it is initialized through a
        // user space system call. So what happens is that the first user space
        // process kicks off the initialization of the console which then
        // allocates a command buffer. That buffer is tagged with the pid of the
        // user space process which then breaks cursor management because the
        // cursor management is done from a kernel vcpu. Thus the pid doesn't
        // match...
        // However we want to move the console out into user space anyway. Bring
        // back the strict check once the console is in user space
        //if ((it->id == id) && (it->ownerPid == pid)) {
        if (it->id == id) {
            return it;
        }
    )
    return NULL;
}



errno_t gdCreateCommandBuffer(pid_t pid, size_t reqSize, gd_cmdbuf_desc_t* _Nonnull desc)
{
    decl_try_err();
    cmdbuf_t* cmdbuf;
    void* opbuf;

    if (reqSize < sizeof(gd_opcode_t) || reqSize > SIZE_KB(8)) {
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


    cmdbuf->id = g_next_cmdimg_id++;
    cmdbuf->ownerPid = pid;
    cmdbuf->byteSize = reqSize;
    cmdbuf->op = opbuf;
    cmdbuf->opEnd =  cmdbuf->op + cmdbuf->byteSize;

    desc->addr = (void*)cmdbuf->op;
    desc->size = cmdbuf->byteSize;
    desc->id = cmdbuf->id;

    deque_add_first(&g_cmdbuf_list, &cmdbuf->chain);

    return EOK;
}

static void _gdDestroyCommandBuffer(cmdbuf_t* _Nonnull cmdbuf)
{
    kfree(cmdbuf->op);
    kfree(cmdbuf);
}

errno_t gdDestroyCommandBuffer(pid_t pid, int id)
{
    if (id == 0) {
        return EOK;
    }

    cmdbuf_t* cmdbuf = _cmdbuf_for_id(pid, id);
    if (cmdbuf == NULL) {
        return EINVAL;
    }

    _gdDestroyCommandBuffer(cmdbuf);

    return EOK;
}

void gdDestroyCommandBuffersOwnedBy(pid_t pid)
{
    deque_for_each(&g_cmdbuf_list, struct cmdbuf, it,
        if (it->ownerPid == pid) {
            _gdDestroyCommandBuffer(it);
        }
    )
}

static errno_t _exec_sprite_cmds(cmdbuf_t* cmdbuf)
{
    decl_try_err();
    const union vio_op* ip = (const union vio_op*)cmdbuf->op;
    size_t ilen;

    while ((const char*)ip < cmdbuf->opEnd) {
        switch (ip->opcode) {
            case GD_OPCODE_END:                // gd_opcode_t
                return EOK;

            case GD_OPCODE_BIND_IMAGE:         // gd_op_bind_image   //XXX will turn into gdCmdSpriteBufferLevel
                try(gdBindImage(cmdbuf->ownerPid, ip->bind_image.target, ip->bind_image.imageId));
                ilen = sizeof(struct gd_op_bind_image);
                break;

            case GD_OPCODE_MOVE_SPRITE:        // struct gd_op_move_sprite
                try(gdMoveSprite(cmdbuf->ownerPid, ip->put_sprite.spriteId, ip->put_sprite.x, ip->put_sprite.y));
                ilen = sizeof(struct gd_op_move_sprite);
                break;

            case GD_OPCODE_SHOW_SPRITE:        // struct gd_op_show_sprite
                try(gdShowSprite(cmdbuf->ownerPid, ip->show_sprite.spriteId, ip->show_sprite.visible != 0));
                ilen = sizeof(struct gd_op_show_sprite);
                break;

            default:
                return EINVAL;
        }

        ip = (const union vio_op*)((const char*)ip + ilen);
    }

catch:
    return err;
}

static errno_t _exec_transfer_cmds(cmdbuf_t* _Nonnull cmdbuf)
{
    const union vio_op* ip = (const union vio_op*)cmdbuf->op;
    image_t* dstbuf;
    size_t ilen;

    while ((const char*)ip < cmdbuf->opEnd) {
        switch (ip->opcode) {
            case GD_OPCODE_END:                // gd_opcode_t
                return EOK;

            case GD_OPCODE_WRITE_PIXELS:       // struct gd_op_write_pixels
                gdWritePixels(cmdbuf->ownerPid, ip->write_pixels.dstImageId, &ip->write_pixels.plane[0], ip->write_pixels.bytesPerRow, ip->write_pixels.format);
                ilen = sizeof(struct gd_op_write_pixels) + (_gdGetPlaneCount(ip->write_pixels.format) - 1) * sizeof(void*);
                break;

            default:
                return EINVAL;
        }

        ip = (const union vio_op*)((const char*)ip + ilen);
    }
}

static errno_t _exec_blit_cmds(cmdbuf_t* _Nonnull cmdbuf)
{
    const union vio_op* ip = (const union vio_op*)cmdbuf->op;
    image_t* dstbuf;
    size_t ilen;

    while ((const char*)ip < cmdbuf->opEnd) {
        switch (ip->opcode) {
            case GD_OPCODE_END:                // gd_opcode_t
                return EOK;

            case GD_OPCODE_CLEAR_PIXELS:       // struct gd_op_clear_pixels
                gdClearPixels(cmdbuf->ownerPid, ip->clear_pixels.dstImageId);
                ilen = sizeof(struct gd_op_clear_pixels);
                break;

            default:
                return EINVAL;
        }

        ip = (const union vio_op*)((const char*)ip + ilen);
    }
}

errno_t gdSubmitCommandBuffer(pid_t pid, int queue_id, int cmds_id)
{
    decl_try_err();
    cmdbuf_t* cmdbuf = _cmdbuf_for_id(pid, cmds_id);

    if (cmdbuf == NULL) {
        return EINVAL;
    }

    switch (queue_id) {
        case GD_BLIT_QUEUE:
            return _exec_blit_cmds(cmdbuf);

        case GD_TRANSFER_QUEUE:
            return _exec_transfer_cmds(cmdbuf);

        case GD_SPRITE_QUEUE:
            return _exec_sprite_cmds(cmdbuf);

        default:
            return EINVAL;
    }
}
