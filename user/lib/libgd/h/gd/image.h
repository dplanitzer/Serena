//
//  gd/image.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_IMAGE_H
#define _GD_IMAGE_H 1

#include <gd/types.h>

extern gd_image_t _Nullable gdCreateImage(gd_device_t _Nonnull device, int width, int height, gd_pixfmt_t format);
extern void gdDestroyImage(gd_image_t _Nullable image);

extern void gdGetImageInfo(gd_image_t _Nonnull image, gd_image_info_t* _Nonnull pOutInfo);
extern void gdMapImage(gd_image_t _Nonnull image, int mode, gd_image_data_t* _Nonnull pOutMapping);
extern void gdUnmapImage(gd_image_t _Nonnull image);

#endif /* _GD_IMAGE_H */
