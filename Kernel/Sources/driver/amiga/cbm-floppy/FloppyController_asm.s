;
;  FloppyController_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/12/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "../hal/chipset.i"
    include "../hal/lowmem.i"

    xdef _fdc_io_begin
    xdef _fdc_io_end
    xdef _fdc_nano_delay


;-------------------------------------------------------------------------------
; void fdc_nano_delay(void)
; Waits a few nanoseconds.
_fdc_nano_delay:
    nop
    nop
    rts

    
;-------------------------------------------------------------------------------
; void fdc_io_begin(FdcControlByte* fdc, uint16_t* data, int nwords, int readwrite)
; Starts an fdc i/o operation to read or write the contents of the track buffer.
_fdc_io_begin:
    inline
    cargs iob_fdc_ptr.l, iob_data.l, iob_nwords.l, iob_readwrite.l
        move.l  iob_fdc_ptr(sp), a0
        move.b  (a0), CIABPRB                   ; make sure that we use our fdc control byte

        lea     CUSTOM_BASE, a1                 ; configure the disk DMA related stuff
        move.l  iob_data(sp), DSKPT(a1)
        move.w  #$7f00, ADKCON(a1)              ; clear disk flags
        move.w  #$9500, ADKCON(a1)              ; MFM, word sync
        move.w  #$4489, DSKSYNC(a1)             ; sync word
        move.w  #$4000, DSKLEN(a1)              ; turn disk DMA off
        move.l  iob_nwords(sp), d0
        move.w  #$8210, DMACON(a1)
        tst.l   iob_readwrite(sp)               ; update the read/write bit
        bne.s   .do_write
        bclr    #14, d0                         ; read from disk
        bra.s   .do_io
.do_write:
        bset    #14, d0                         ; write to disk

.do_io:
        bset    #15, d0
        move.w  d0, DSKLEN(a1)                  ; turn disk DMA on
        move.w  d0, DSKLEN(a1)
        rts
    einline


;-------------------------------------------------------------------------------
; void fdc_io_end(FdcControlByte* fdc)
; Stops the currently active fdc i/o operation (marks it as done).
_fdc_io_end:
    inline
    cargs ioe_fdc_ptr.l
        lea     CUSTOM_BASE, a0
        move.w  #$4000, DSKLEN(a0)              ; turn disk DMA off
        move.w  #$10, DMACON(a0)

        or.b    #%01111000, d0                  ; deselect all drives
        move.b  d0, CIABPRB

        rts
    einline

