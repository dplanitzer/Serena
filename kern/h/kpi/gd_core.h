//
//  kpi/gd_core.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_GD_CORE_H
#define _KPI_GD_CORE_H 1

#include <_cmndef.h>
#include <kpi/gd_types.h>
#include <kpi/ioctl.h>

//
// Images
//

// Creates a 2d image with size 'width' x 'height' pixels and pixel format
// 'format'. Returns the unique image ID in 'pOutId' on success. Note
// that the image width and height have to be > 1. The image may be directly
// mapped into the address space of the owning process or manipulated with the
// Blitter.
// gdCreateImage(int width, int height, gd_pixfmt_t format, int* _Nonnull pOutId)
#define GDC_CREATE_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 1, _IOCMD_ACC_WR, 0)

// Destroys the image with id 'id'. Returns EBUSY if the image is currently
// mapped. Automatically unbinds the image if it is bound to a sprite and binds
// the sprite target to a null image. Does nothing if 'id' is 0.
// gdcDestroyImage(int id)
#define GDC_DESTROY_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 2, _IOCMD_ACC_WR, 0)

// Returns information about the image with ID 'id'.
// gdcGetImageInfo(int id, gd_image_info_t* _Nonnull pOutInfo)
#define GDC_GET_IMAGE_INFO \
IOCMD_MAKE(IOPROTO_FB, 3, _IOCMD_ACC_RD, 0)

// Maps the pixels of the image 'id' into the address space of the calling
// process to allow direct access to the pixel data. 'mode' specifies
// whether the pixel data should be mapped for reading only or reading and
// writing. Returns with 'pOutMapping' filled in.
// gdcMapImage(int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
#define GDC_MAP_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 4, _IOCMD_ACC_RDWR, 0)

// Unmaps the pixels of the image 'id' and revokes access to the pixels.
// gdcUnmapImage(int id)
#define GDC_UNMAP_IMAGE \
IOCMD_MAKE(IOPROTO_FB, 5, _IOCMD_ACC_RDWR, 0)


//
// Sprites
//

// Acquires 'count' sprites with ascending priorities and returns the ids in
// the array 'pOutSpriteIds'. The array must have 'count' entries.
// If 'basePriority' is >= 0 then 'count' sprites with consecutive ascending
// priorities are allocated. Each sprite has a priority +1 greater than the
// previous sprite. The allocation will only succeed if 'count' consecutive
// sprite priorities are available. Otherwise the allocation fails and no sprites
// are allocated.
// 'If 'basePriority' is < 0 then 'count' sprites with an arbitrary and available
// priority are allocated.
// EAGAIN is returned if not enough sprites are available.
// gdcAcquireSprites(int basePriority, size_t count, int* _Nonnull pOutSpriteIds)
#define GDC_ACQUIRE_SPRITES \
IOCMD_MAKE(IOPROTO_FB, 6, _IOCMD_ACC_WR, 0)

// Releases 'count' sprites owned by the calling process. The sprite ids are
// provided by the 'spriteIds' array. All sprites currently owned by the calling
// process are released if 'spriteIds' is NULL.
// gdcReleaseSprites(const int* _Nullable spriteIds, size_t count)
#define GDC_RELEASE_SPRITES \
IOCMD_MAKE(IOPROTO_FB, 7, _IOCMD_ACC_WR, 0)

// Returns information about the limits of the sprite sub-system based on the
// currently active display.
// gdcGetSpriteConstraints(gd_sprite_constraints_t* _Nonnull constraints)
#define GDC_SPRITE_CONSTRAINTS \
IOCMD_MAKE(IOPROTO_FB, 10, _IOCMD_ACC_RD, 0)


//
// Display
//

// Switches the display to the display mode 'mode' and applies the dynamic
// display parameters 'params' if specified. The switch is executed on the next
// VBL and the caller is blocked until teh switch has completed. 'op' specifies
// how teh switch should be executed:
// GD_APPLY - the switch is executed.
// GD_CHECK - the call verifies whether the switch would succeed but it does not
//            actually execute it. EOK is returned if the switch would be successful
//            and a suitable error is returned if it would fail.
// gdDisplayMode(const gd_display_mode_t* _Nonnull mode, const gd_display_params* _Nullable params, int op)
#define GDC_DISPLAY_MODE \
IOCMD_MAKE(IOPROTO_FB, 11, _IOCMD_ACC_WR, 0)

// Returns the display info indicated by 'flavor'.
// gdGetDisplayInfo(int flavor, gd_display_info_ref _Nonnull pOutInfo)
#define GDC_GET_DISPLAY_INFO \
IOCMD_MAKE(IOPROTO_FB, 12, _IOCMD_ACC_RD, 0)

// Returns the display mode with index 'index'. Returns EOK if a display mode
// with such an index exists and EINVAL if not. Call this function with index 0
// as teh first index and then with previous index + 1 until it returns EINVAL
// to get all supported display modes.
// gdEnumDisplayModes(int index, gd_display_mode_t* _Nonnull pOutMode)
#define GDC_ENUM_DISPLAY_MODES \
IOCMD_MAKE(IOPROTO_FB, 13, _IOCMD_ACC_RD, 0)


//
// CLUT
//

// Replaces the color entries of the CLUT starting at entry 'idx' up to entry
// 'idx + count'. The provided color values are converted to the color resolution
// that is actually supported by the CLUT. The color values will become visible
// on the screen starting with the next VBL.
// gdClut(size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
#define GDC_CLUT \
IOCMD_MAKE(IOPROTO_FB, 14, _IOCMD_ACC_WR, 0)

// Returns a copy of the CLUT entries from 'idx' to 'idx + count'. The returned
// color values represent the physical CLUT color values. They may have reduced
// color precision compared to the color values that were originally set by a
// gdClut() command. Use gdGetClutInfo() receive information about the
// supported CLUT color resolution.
// gdGetClut(size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
#define GDC_GET_CLUT \
IOCMD_MAKE(IOPROTO_FB, 15, _IOCMD_ACC_RD, 0)

// Returns information about the display CLUT. The number of color entries and
// the physical color resolution is returned.
// gdGetClutInfo(gd_clut_info_t* _Nonnull info)
#define GDC_GET_CLUT_INFO \
IOCMD_MAKE(IOPROTO_FB, 16, _IOCMD_ACC_RD, 0)



//
// Command Buffers
//

// Allocates a command buffer and maps it for reading and writing into the
// address space of the calling process. 'size' is the requested size of the
// buffer. The call returns the base address and the actual buffer size in
// 'desc' when successful. Note that the actual size may be greater than the
// requested size. However it will never be smaller.
// create_cmdbuf(size_t byteSize, const gd_cmdbuf_desc_t* _Nullable desc) -> id
#define GDC_CREATE_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 17, _IOCMD_ACC_WR, 0)

// Deallocates the command buffer 'id'.
// destroy_cmdbuf(int id)
#define GDC_DESTROY_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 18, _IOCMD_ACC_WR, 0)

// Submits the command buffer 'cmds_id' to the command queue 'queue_id' for
// execution. Depending on the targeted queue, the command may be executed
// asynchronously or synchronously. Malformed commands are ignored and execution
// is terminated if either an end command is encountered or a malformed command
// is encountered from which the command queue can not recover.
// gdSubmitCommands(int queue_id, int cmds_id)
#define GDC_SUBMIT_CMDBUF \
IOCMD_MAKE(IOPROTO_FB, 19, _IOCMD_ACC_WR, 0)

#endif /* _KPI_GD_CORE_H */
