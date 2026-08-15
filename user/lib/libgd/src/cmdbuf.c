//
//  cmdbuf.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <gd/cmdbuf.h>
#include <stdlib.h>


gd_cmdbuf_t _Nullable gdCreateCommandBuffer(gd_device_t _Nonnull device, size_t byteSize)
{
    struct gd_device* dp = device;
    gd_cmdbuf_desc_t desc;

    if (fd_cntl(dp->fd, GDC_CREATE_CMDBUF, byteSize, &desc) < 0) {
        return NULL;
    }


    struct gd_cmdbuf* cbp = calloc(1, sizeof(struct gd_cmdbuf));
    if (cbp == NULL) {
        fd_cntl(dp->fd, GDC_DESTROY_CMDBUF, desc.id);
        return NULL;
    }


    cbp->dev_d = dp->fd;
    cbp->cmdbuf_d = desc.id;
    cbp->size = desc.size;
    cbp->base_addr = desc.addr;
    cbp->end_addr = cbp->base_addr + cbp->size;
    cbp->next_addr = cbp->base_addr;

    return cbp;
}

void gdDestroyCommandBuffer(gd_cmdbuf_t _Nullable cmdbuf)
{
    struct gd_cmdbuf* cbp = cmdbuf;

    if (cbp) {
        fd_cntl(cbp->dev_d, GDC_DESTROY_CMDBUF, cbp->cmdbuf_d);
        free(cbp);
    }
}

void gdSubmitCommandBuffer(gd_cmdbuf_t _Nonnull cmdbuf, int queue_id)
{
    struct gd_cmdbuf* cbp = cmdbuf;

    fd_cntl(cbp->dev_d, GDC_SUBMIT_CMDBUF, queue_id, cbp->cmdbuf_d);
}
