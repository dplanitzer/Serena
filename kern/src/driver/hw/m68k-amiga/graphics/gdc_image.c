//
//  gdc_image.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gdc_priv.h"
#include "video_conf.h"
#include <ext/math.h>
#include <string.h>
#include <kern/kalloc.h>
#include <kpi/process.h>

static int      g_next_image_id = GD_DYNAMIC_IMAGE_BASE;
static deque_t  g_image_table;


static errno_t _alloc_single_plane(image_t* _Nonnull self)
{
    decl_try_err();
    size_t nbytes;

    switch (self->pixelFormat) {
        case GD_RGB_SPRITE_2:
            // sprxctl, sprxctl, (plane0, plane1)..., 0, 0
            nbytes = 2*sizeof(uint16_t) + (2*self->height*self->bytesPerRow) + 2*sizeof(uint16_t);
            break;

        default:
            nbytes = self->bytesPerRow * self->height;
            break;
    }

    try(kalloc_options(nbytes, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0]));

    if (self->pixelFormat == GD_RGB_SPRITE_2) {
        uint16_t* p = (uint16_t*)self->plane[0];

        p[0] = 0;
        p[1] = 0;
        p[2 + (self->height << 1) + 0] = 0;
        p[2 + (self->height << 1) + 1] = 0;
    }

catch:
    return err;
}

static errno_t _alloc_multi_plane(image_t* _Nonnull self)
{
    decl_try_err();
    const size_t bytesPerPlane = self->bytesPerRow * self->height;
    const size_t bytesPerClusteredPlane = __Ceil_PowerOf2(bytesPerPlane, 4);
    const size_t clusteredSize = self->planeCount * bytesPerClusteredPlane;

    // Allocate the planes. Note that we try to cluster the planes whenever possible.
    // This means that we allocate a single contiguous memory range big enough to
    // hold all planes. We only allocate independent planes if we're not able to
    // allocate a big enough contiguous memory region because DMA memory has become
    // too fragmented to pull this off. Individual planes in a clustered planes
    // configuration are aligned on an 4 byte boundary.

    err = kalloc_options(clusteredSize, KALLOC_OPTION_UNIFIED, (void**) &self->plane[0]);
    if (err == EOK) {
        for (int8_t i = 1; i < self->planeCount; i++) {
            self->plane[i] = self->plane[i - 1] + bytesPerClusteredPlane;
        }
    }

    return err;
}

static void _destroy(image_t* _Nonnull self)
{
    _gdcConcealImage(self);
    kfree(self->plane[0]);
    kfree(self);
}


errno_t _gdcCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, image_t* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    image_t* self;
    
    if (width <= 0 || height <= 0) {
        return EINVAL;
    }


    try(kalloc_cleared(sizeof(image_t), (void**) &self));
    
    self->ownerPid = pid;
    self->refCount = 1;
    self->pixelFormat = pixelFormat;
    self->width = width;
    self->height = height;
    self->bytesPerRow = ((width + 15) >> 4) << 1;       // Must be a multiple of at least words (16bits)
    self->planeCount = _gdGetPlaneCount(pixelFormat);


    if (self->planeCount == 1) {
        try(_alloc_single_plane(self));
    }
    else {
        try(_alloc_multi_plane(self));
    }
    

    *pOutSelf = self;
    return EOK;
    
catch:
    _destroy(self);
    return err;
}

// Make the image publicly accessible by 'id'
void _gdcPublishImage(image_t* _Nonnull self, int id)
{
    deque_add_first(&g_image_table, &self->chain);
    self->id = id;
}

// Make the image no longer publicly accessible by its id
void _gdcConcealImage(image_t* _Nonnull self)
{
    if (self->id != 0) {
        self->id = 0;
        deque_remove(&g_image_table, &self->chain);
    }
}

errno_t gdcCreateImage(pid_t pid, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    image_t* self;

    const errno_t err = _gdcCreateImage(pid, width, height, pixelFormat, &self);
    if (err == EOK) {
        _gdcPublishImage(self, g_next_image_id++);
        *pOutId = _gdcGetImageId(self);
    }
    return err;
}

image_t* _Nullable _gdcGetImageById(pid_t pid, int id)
{
    deque_for_each(&g_image_table, image_t, it,
        // Image owned by process or the kernel is fine. Kernel owned images are
        // things like framebuffer, mouse cursor, etc
        if ((it->id == id) && (it->ownerPid == pid || it->ownerPid == PID_KERNELD)) {
            return it;
        }
    )
    return NULL;
}

void _gdcReleaseImage(image_t* _Nullable self)
{
    if (self) {
        self->refCount--;
        if (self->refCount == 0) {
            _destroy(self);
        }
    }
}

void _gdcDestroyImage(image_t* _Nonnull self)
{
    // Unschedule an upcoming Copper program first, to make sure that the currently
    // running Copper program can't change on us while we're inspecting it. GD is
    // locked too so nobody else can trigger the scheduling of another Copper
    // program while we're here.
    copper_prog_t next_prog = copper_unschedule();
    bool bNeedEditableCopperProg = false;


    for (int i = 0; i < SPRITE_COUNT; i++) {
        if (g_sprite[i].image == self) {
            bNeedEditableCopperProg = true;
            break;
        }
    }

    if (bNeedEditableCopperProg && !next_prog) {
        next_prog = copper_get_editable_prog();
    }


    for (int i = 0; i < SPRITE_COUNT; i++) {
        sprite_channel_t* spr = &g_sprite[i];

        if (spr->image == self) {
            _bind_sprite_image(spr, NULL);
            copper_prog_sprptr_changed(next_prog, spr->id, (spr->image && spr->isVisible) ? spr->image : NULL);
        }
    }


    _gdcConcealImage(self);
    _gdcReleaseImage(self);

    if (next_prog) {
        copper_schedule(next_prog, 0);
    }
}

errno_t gdcDestroyImage(pid_t pid, int id)
{
    switch (id) {
        case 0:
            return EOK;

        case GD_FRONT_BUFFER:
            return EINVAL;

        default:
            break;
    }


    image_t* self = _gdcGetImageById(pid, id);
    if (self) {
        if (_gdcIsImageMapped(self)) {
            return EBUSY;
        }

        _gdcDestroyImage(self);
        return EOK;
    } else {
        return EINVAL;
    }
}

void gdcDestroyImagesOwnedBy(pid_t pid)
{
    deque_for_each(&g_image_table, image_t, it,
        if (it->ownerPid == pid) {
            _gdcDestroyImage(it);
        }
    )
}

errno_t gdcGetImageInfo(pid_t pid, int id, gd_image_info_t* _Nonnull pOutInfo)
{
    image_t* self = _gdcGetImageById(pid, id);

    if (self == NULL) {
        return EINVAL;
    }

    pOutInfo->width = _gdcGetImageWidth(self);
    pOutInfo->height = _gdcGetImageHeight(self);
    pOutInfo->pixelFormat = _gdcGetImagePixelFormat(self);

    return EOK;
}

errno_t gdcMapImage(pid_t pid, int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
{
    image_t* self = _gdcGetImageById(pid, id);

    if (self == NULL) {
        return EINVAL;
    }
    if (_gdcIsImageMapped(self)) {
        return EBUSY;
    }
    if (_gdcGetImagePixelFormat(self) == GD_RGB_SPRITE_2) {
        // Disallow mapping sprite surfaces for now
        return ENOTSUP;
    }

    pOutMapping->planeCount = _gdcGetImagePlaneCount(self);
    pOutMapping->bytesPerRow = _gdcGetImageBytesPerRow(self);
    for (size_t i = 0; i < pOutMapping->planeCount; i++) {
        pOutMapping->plane[i] = _gdcGetImagePlane(self, i);
    }

    self->flags |= GD_IMAGE_MAPPED;

    return EOK;
}

errno_t gdcUnmapImage(pid_t pid, int id)
{
    image_t* self = _gdcGetImageById(pid, id);

    if (self == NULL) {
        return EINVAL;
    }


    if (_gdcIsImageMapped(self)) {
        self->flags &= ~GD_IMAGE_MAPPED;
        return EOK;
    }
    else {
        return EPERM;
    }
}

errno_t _gdcWritePixels(image_t* self, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format)
{
    if (self->pixelFormat == GD_RGB_SPRITE_2 && format == GD_COLOR_INDEX2) {
        const uint8_t* sp0 = planes[0];
        const uint8_t* sp1 = planes[1];
        uint16_t* pp = (uint16_t*)self->plane[0];
        uint16_t* dp = &pp[2];

        for (int y = 0; y < self->height; y++) {
            *dp++ = *(uint16_t*)sp0; sp0 += bytesPerRow;
            *dp++ = *(uint16_t*)sp1; sp1 += bytesPerRow;
        }

        return EOK;
    }
    else if (self->pixelFormat == format) {
        for (int8_t p = 0; p < self->planeCount; p++) {
            const uint8_t* sp = planes[p];
            uint8_t* dp = self->plane[p];

            for (int y = 0; y < self->height; y++) {
                memcpy(dp, sp, self->width >> 3);
                dp += self->bytesPerRow;
                sp += bytesPerRow;
            }
        }

        return EOK;
    }
    else {
        return ENOTSUP;
    }
}

errno_t gdcWritePixels(pid_t pid, int id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format)
{
    image_t* pbo = _gdcGetImageById(pid, id);

    if (pbo == NULL) {
        return EINVAL;
    }

    return _gdcWritePixels(pbo, planes, bytesPerRow, format);
}

void _gdcClearPixels(image_t* _Nonnull self)
{
    if (self->pixelFormat == GD_RGB_SPRITE_2) {
        uint16_t* pp = (uint16_t*)self->plane[0];
        uint16_t* dp = &pp[2];

        for (int y = 0; y < self->height; y++) {
            *dp++ = 0;
            *dp++ = 0;
        }
    }
    else {
        for (int8_t p = 0; p < self->planeCount; p++) {
            memset(self->plane[p], 0, self->bytesPerRow * self->height);
        }
    }
}

errno_t gdcClearPixels(pid_t pid, int id)
{
    image_t* self = _gdcGetImageById(pid, id);

    if (self == NULL) {
        return EINVAL;
    }

    _gdcClearPixels(self);
    return EOK;
}
