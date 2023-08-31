;
;  start_m68k.s
;  Apollo
;
;  Created by Dietmar Planitzer on 8/30/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    xref ___stdlibc_init
    xref _main
    xref _exit

    xdef _start


;-------------------------------------------------------------------------------
; void start(void)
_start:
    ; Call void __stdlibc_init()
    jsr     ___stdlibc_init
    
    ; build an empty command line
    ; XXX get the real command line
    move.l  #0, -(sp)

    ; Call int main(int argc, char *argv[])
    pea     (sp)
    move.l  #0, -(sp)
    jsr     _main
    add.w   #8, sp

    ; Call exit()
    move.l  d0, -(sp)
    jsr     _exit
    ; not reached

