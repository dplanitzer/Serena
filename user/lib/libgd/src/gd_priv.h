//
//  gd/gd_priv.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_PRIV_H
#define _GD_PRIV_H 1

#include <gd/types.h>
#include <kpi/gd_core.h>
#include <serena/fd.h>

struct gd_device {
    int fd;
};


struct gd_image {
    int dev_d;
    int img_d;
};


struct gd_sprite {
    int dev_d;
    int sprite_d;
};


struct gd_cmdbuf {
    int             dev_d;
    int             cmdbuf_d;
    size_t          size;
    char* _Nonnull  base_addr;      // first byte in the command buffer
    char* _Nonnull  end_addr;       // first byte after end of command buffer
    char* _Nonnull  next_addr;      // where tp lay down the next command
};

#endif /* _GD_PRIV_H */
