;
;  chipset_denise_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/8/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"

    xdef _denise_stop_video_refresh
    xdef _denise_set_clut_entry


;-------------------------------------------------------------------------------
; void denise_stop_video_refresh(void)
; Turns the video refresh DMA off
_denise_stop_video_refresh:
    move.w  #(DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE), CUSTOM_BASE+DMACON
    rts


;-------------------------------------------------------------------------------
; void denise_set_clut_entry(Int index, UInt16 color)
_denise_set_clut_entry:
    cargs sce_index.l, sce_color.l
    lea     CUSTOM_BASE + COLOR_BASE, a0
    move.l  sce_index(sp), d0
    lsl.l   #1, d0
    add.l   d0, a0
    move.l  sce_color(sp), d0
    move.w  d0, (a0)
    rts
