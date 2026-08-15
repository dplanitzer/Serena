//
//  image.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <gd/image.h>
#include <stdlib.h>


gd_image_t _Nullable gdCreateImage(gd_device_t _Nonnull device, int width, int height, gd_pixfmt_t format)
{
    struct gd_device* dp = device;
    struct gd_image* ip = calloc(1, sizeof(struct gd_image));

    if (ip == NULL) {
        return NULL;
    }

    ip->dev_d = dp->fd;
    if (fd_cntl(dp->fd, GDC_CREATE_IMAGE, width, height, format, &ip->img_d) < 0) {
        free(ip);
        return NULL;
    }

    return ip;
}

void gdDestroyImage(gd_image_t _Nullable image)
{
    struct gd_image* ip = image;

    if (ip) {
        fd_cntl(ip->dev_d, GDC_DESTROY_IMAGE, ip->img_d);
        free(ip);
    }
}

void gdGetImageInfo(gd_image_t _Nonnull image, gd_image_info_t* _Nonnull pOutInfo)
{
    struct gd_image* ip = image;

    fd_cntl(ip->dev_d, GDC_GET_IMAGE_INFO, ip->img_d, pOutInfo);
}

void gdMapImage(gd_image_t _Nonnull image, int mode, gd_image_data_t* _Nonnull pOutMapping)
{
    struct gd_image* ip = image;

    fd_cntl(ip->dev_d, GDC_MAP_IMAGE, ip->img_d, mode, pOutMapping);
}

void gdUnmapImage(gd_image_t _Nonnull image)
{
    struct gd_image* ip = image;

    fd_cntl(ip->dev_d, GDC_UNMAP_IMAGE, ip->img_d);
}
