//
//  gd/cmdbuf.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_CMDBUF_H
#define _GD_CMDBUF_H 1

#include <gd/types.h>

extern gd_cmdbuf_t _Nullable gdCreateCommandBuffer(gd_device_t _Nonnull device, size_t byteSize);
extern void gdDestroyCommandBuffer(gd_cmdbuf_t _Nullable cmdbuf);

extern void gdSubmitCommandBuffer(gd_cmdbuf_t _Nonnull cmdbuf, int queue_id);

#endif /* _GD_CMDBUF_H */
