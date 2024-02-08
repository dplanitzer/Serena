;
;  FloppyDisk_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/12/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include "lowmem.i"

    xdef _fdc_get_drive_status
    xdef _fdc_set_drive_motor
    xdef _fdc_step_head
    xdef _fdc_select_head
    xdef _fdc_io_begin
    xdef _fdc_get_io_status
    xdef _fdc_io_end


;-------------------------------------------------------------------------------
; unsigned int fdc_get_drive_status(FdcControlByte* fdc)
; Returns the status word of the floppy disk controller.
_fdc_get_drive_status:
    inline
    cargs gds_fdc_ptr.l
        move.l  gds_fdc_ptr(sp), a0
        move.b  (a0), d1                    ; make sure that we use our fdc control byte and that our drive is selected
        move.b  d1, CIABPRB

        clr.l   d0                          ; read the fdc status
        move.b  CIAAPRA, d0

        or.b    #%01111000, d1              ; make sure that we leave the drives in a deselected state
        move.b  d1, CIABPRB

        rts
    einline


;-------------------------------------------------------------------------------
; void fdc_set_drive_motor(FdcControlByte* fdc int onoff)
; Turns the drive motor on or off.
_fdc_set_drive_motor:
    inline
    cargs sdm_fdc_ptr.l, sdm_onoff.l
        move.l  sdm_fdc_ptr(sp), a0
        move.l  sdm_onoff(sp), d0

        or.b    #%01111000, d1              ; deselect all drives
        move.b  d1, CIABPRB

        move.b  (a0), d1                    ; make sure that we use our fdc control byte
        tst.l   d0                          ; update the motor bit
        beq.s   .motor_off
        bclr    #7, d1
        bra.s   .latch_motor_state
.motor_off:
        bset    #7, d1
.latch_motor_state:
        move.b  d1, CIABPRB                 ; this triggers the motor switch in the hardware
        move.b  d1, (a0)                    ; update the fdc control byte shadow copy

        or.b    #%01111000, d1              ; deselect all drives
        move.b  d1, CIABPRB

        rts
    einline


;-------------------------------------------------------------------------------
; void fdc_step_head(FdcControlByte* fdc, int inout)
; Steps the drive head one cylinder towards the inside (+1) or the outside (-1)
; of the drive.
_fdc_step_head:
    inline
    cargs sh_fdc_ptr.l, sh_inout.l
        move.l  sh_fdc_ptr(sp), a0
        move.b  (a0), d0                    ; make sure that we use our fdc control byte

        tst.l   sh_inout(sp)                ; compute direction bit
        bmi.s   .dir_outside
        bclr    #1, d0                      ; step inward
        bra.s   .do_step
.dir_outside:
        bset    #1, d0                      ; step outward

.do_step:
        move.b  d0, (a0)
        bset    #0, d0                      ; pulse dskstep high
        move.b  d0, CIABPRB
        nop
        nop
        bclr    #0, d0                      ; pulse dskstep low
        move.b  d0, CIABPRB
        nop
        nop
        bset    #0, d0                      ; pulse dskstep high again
        move.b  d0, CIABPRB

        or.b    #%01111000, d0              ; deselect all drives
        move.b  d0, CIABPRB

        rts
    einline


;-------------------------------------------------------------------------------
; void fdc_select_head(FdcControlByte* fdc, int side)
; Selects the disk head. 0 (lower head) or 1 (upper head).
_fdc_select_head:
    inline
    cargs seh_fdc_ptr.l, seh_head.l
        move.l  seh_fdc_ptr(sp), a0
        move.b  (a0), d0                    ; make sure that we use our fdc control byte

        tst.l   seh_head(sp)                ; update the dskside bit
        beq.s   .1
        bclr    #2, d0
        bra.s   .2
.1:
        bset    #2, d0
.2:
        move.b  d0, (a0)
        move.b  d0, CIABPRB

        or.b    #%01111000, d0              ; deselect all drives
        move.b  d0, CIABPRB

        rts
    einline


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
; unsigned int fdc_get_io_status(FdcControlByte* fdc)
; Returns true if the currently active fdc_io_begin() call is done.
_fdc_get_io_status:
    inline
    cargs ios_fdc_ptr.l
        move.b  CIAAPRA, d1                 ; read the fdc status
        tst.l   d0
        beq.s   .io_not_done
        bset    #0, d1
        bra.s   .io_status_return
.io_not_done:
        bclr    #0, d1
.io_status_return:
        clr.l   d0
        move.b  d1, d0
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

