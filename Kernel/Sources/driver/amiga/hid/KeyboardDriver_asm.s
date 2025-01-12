
;
;  KeyboardDriver_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 5/26/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "../hal/chipset.i"
    include "../hal/lowmem.i"

    xdef _ksb_init
    xdef _ksb_receive_key
    xdef _ksb_acknowledge_key


;-------------------------------------------------------------------------------
; void ksb_init(void)
; Prepares the CIA serial port for reception of key presses.
_ksb_init:
    move.b  #0, CIAACRA
    rts

;-------------------------------------------------------------------------------
; int ksb_receive_key(void)
; Returns the key that was pressed / released and starts the acknowledge cycle.
; Uses CIA A timer A for the acknowledgement countdown
_ksb_receive_key:
    move.b  CIAASDR, d0
    move.b  #61, CIAATALO
    move.b  #0, CIAATAHI
    move.b  #%01011001, CIAACRA
    not.b   d0
    ror.b   #1,d0
    ext.w   d0
    ext.l   d0
    rts

;-------------------------------------------------------------------------------
; void ksb_acknowledge_key(void)
; Acknowledges the most recent key press / release and switches back into receive
; mode.
_ksb_acknowledge_key:
    btst.b  #0, CIAACRA
    bne.s   _ksb_acknowledge_key

    and.b   #%10111111, CIAACRA
    rts
