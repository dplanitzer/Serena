;
;  arch/m68k/strcpy_x.s
;  libc, libsc
;
;  Created by Dietmar Planitzer on 2/28/26.
;  Copyright © 2026 Dietmar Planitzer. All rights reserved.
;

    xdef _strcpy_x


;-------------------------------------------------------------------------------
; char * _Nonnull strcpy_x(char * _Nonnull _Restrict dst, const char * _Nonnull _Restrict src)
;
; Similar to strcpy() but returns a pointer that points to the '\0 at the
; destination aka the end of the copied string. Exists so that we can actually
; use this and strcat() to compose strings without having to iterate over the
; same string multiple times.
_strcpy_x:
    cargs dst.l, src.l

    move.l  src(sp), a0
    move.l  dst(sp), a1

.1:
    move.b  (a0)+, d0
    beq.s   .2
    move.b  d0, (a1)+
    bra.s   .1

.2:
    move.b  d0, (a1)
    move.l  a1, d0
    rts
