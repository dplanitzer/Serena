;
;  GraphicsDriver_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/8/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"

    xdef __GraphicsDriver_StopVideoRefresh
    xdef __GraphicsDriver_SetClutEntry


;-------------------------------------------------------------------------------
; void _GraphicsDriver_StopVideoRefresh(void)
; Turns the video refresh DMA off
__GraphicsDriver_StopVideoRefresh:
    move.w  #(DMAF_COPPER | DMAF_RASTER | DMAF_SPRITE), CUSTOM_BASE+DMACON
    rts


;-------------------------------------------------------------------------------
; void _GraphicsDriver_SetClutEntry(Int index, UInt16 color)
__GraphicsDriver_SetClutEntry:
    cargs sce_index.l, sce_color.l
    lea     CUSTOM_BASE + COLOR_BASE, a0
    move.l  sce_index(sp), d0
    lsl.l   #1, d0
    add.l   d0, a0
    move.l  sce_color(sp), d0
    move.w  d0, (a0)
    rts
