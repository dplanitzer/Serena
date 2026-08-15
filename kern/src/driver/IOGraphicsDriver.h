//
//  IOGraphicsDriver_h.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/12/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef IOGraphicsDriver_h_h
#define IOGraphicsDriver_h_h

#include <driver/IODriver.h>
#include <kpi/gd_core.h>


open_class(IOGraphicsDriver, IODriver,
);
open_class_funcs(IOGraphicsDriver, IODriver,

    //
    // Image
    //

    errno_t (*createImage)(void* _Nonnull self, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId);
    errno_t (*destroyImage)(void* _Nonnull self, int id);
    errno_t (*getImageInfo)(void* _Nonnull self, int id, gd_image_info_t* _Nonnull pOutInfo);
    errno_t (*mapImage)(void* _Nonnull self, int id, int mode, gd_image_data_t* _Nonnull pOutMapping);
    errno_t (*unmapImage)(void* _Nonnull self, int id);


    //
    // Sprites
    //

    errno_t (*acquireSprites)(void* _Nonnull self, int basePriority, size_t count, int* _Nonnull pOutSpriteIds);
    errno_t (*releaseSprites)(void* _Nonnull self, const int* _Nullable spriteIds, size_t count);
    void (*getSpriteConstraints)(void* _Nonnull self, gd_sprite_constraints_t* _Nonnull cp);


    //
    // Command Buffers
    //

    errno_t (*createCommandBuffer)(void* _Nonnull self, size_t size, gd_cmdbuf_desc_t* _Nonnull desc);
    errno_t (*destroyCommandBuffer)(void* _Nonnull self, int id);
    errno_t (*submitCommandBuffer)(void* _Nonnull self, int queue_id, int cmds_id);


    //
    // CLUT
    //

    errno_t (*clut)(void* _Nonnull self, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
    errno_t (*getClut)(void* _Nonnull self, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
    errno_t (*getClutInfo)(void* _Nonnull self, gd_clut_info_t* _Nonnull info);


    //
    // Display
    //

    errno_t (*displayMode)(void* _Nonnull self, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
    errno_t (*getDisplayInfo)(void* _Nonnull self, int flavor, gd_display_info_ref _Nonnull info);
    errno_t (*enumDisplayModes)(void* _Nonnull self, int index, gd_display_mode_t* _Nonnull pOutMode);
);


// Images
#define IOGraphicsDriver_CreateImage(__self, __width, __height, __pixelFormat, __pOutId) \
invoke_n(createImage, IOGraphicsDriver, __self, __width, __height, __pixelFormat, __pOutId)

#define IOGraphicsDriver_DestroyImage(__self, __id) \
invoke_n(destroyImage, IOGraphicsDriver, __self, __id)

#define IOGraphicsDriver_GetImageInfo(__self, __id, __pOutInfo) \
invoke_n(getImageInfo, IOGraphicsDriver, __self, __id, __pOutInfo)

#define IOGraphicsDriver_MapImage(__self, __id, __mode, __pOutMapping) \
invoke_n(mapImage, IOGraphicsDriver, __self, __id, __mode, __pOutMapping)

#define IOGraphicsDriver_UnmapImage(__self, __id) \
invoke_n(unmapImage, IOGraphicsDriver, __self, __id)


// Sprites
#define IOGraphicsDriver_AcquireSprites(__self, __basePriority, __count, __pOutSpriteIds) \
invoke_n(acquireSprites, IOGraphicsDriver, __self, __basePriority, __count, __pOutSpriteIds)

#define IOGraphicsDriver_ReleaseSprites(__self, __spriteIds, __count) \
invoke_n(releaseSprites, IOGraphicsDriver, __self, __spriteIds, __count)

#define IOGraphicsDriver_GetSpriteConstraints(__self, __cp) \
invoke_n(getSpriteConstraints, IOGraphicsDriver, __self, __cp)


// Command buffers
#define IOGraphicsDriver_CreateCommandBuffer(__self, __size, __desc) \
invoke_n(createCommandBuffer, IOGraphicsDriver, __self, __size, __desc)

#define IOGraphicsDriver_DestroyCommandBuffer(__self, __id) \
invoke_n(destroyCommandBuffer, IOGraphicsDriver, __self, __id)

#define IOGraphicsDriver_SubmitCommandBuffer(__self, __queue_id, __cmds_id) \
invoke_n(submitCommandBuffer, IOGraphicsDriver, __self, __queue_id, __cmds_id)


// CLUT
#define IOGraphicsDriver_Clut(__self, __idx, __count, __entries) \
invoke_n(clut, IOGraphicsDriver, __self, __idx, __count, __entries)

#define IOGraphicsDriver_GetClut(__self, __idx, __count, __entries) \
invoke_n(getClut, IOGraphicsDriver, __self, __idx, __count, __entries)

#define IOGraphicsDriver_GetClutInfo(__self, __info) \
invoke_n(getClutInfo, IOGraphicsDriver, __self, __info)


// Display
#define IOGraphicsDriver_DisplayMode(__self, __mode, __params, __op) \
invoke_n(displayMode, IOGraphicsDriver, __self, __mode, __params, __op)

#define IOGraphicsDriver_GetDisplayInfo(__self, __flavor, __info) \
invoke_n(getDisplayInfo, IOGraphicsDriver, __self, __flavor, __info)

#define IOGraphicsDriver_EnumDisplayModes(__self, __index, __pOutMode) \
invoke_n(enumDisplayModes, IOGraphicsDriver, __self, __index, __pOutMode)


// In-kernel command buffer utilities (XXX tmp)
extern void* _Nonnull gdCmdWritePixels(void* _Nonnull addr, int img_id, const void* _Nonnull planes[], size_t bytesPerRow, gd_pixfmt_t format);
extern void* _Nonnull gdCmdClearPixels(void* _Nonnull addr, int img_id);
extern void* _Nonnull gdCmdBindSpriteImage(void* _Nonnull addr, int spr_id, int img_id);
extern void* _Nonnull gdCmdMoveSprite(void* _Nonnull addr, int spr_id, int16_t x, int16_t y);
extern void* _Nonnull gdCmdShowSprite(void* _Nonnull addr, int spr_id, bool isVisible);
extern void* _Nonnull gdCmdEnd(void* _Nonnull addr);

#endif /* IOGraphicsDriver_h */
