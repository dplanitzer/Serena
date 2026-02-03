;
;  machine/hw/m68k/cpu_asm.s
;  kernel
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include <hal/hw/m68k-amiga/chipset.i>
    include <hal/hw/m68k/cpu.i>
    include <hal/hw/m68k/lowmem.i>

    xref _cpu_non_recoverable_error

    xdef _cpu_get_model
    xdef _cpu_verify_ram_4b
    xdef _cpu_guarded_read
    xdef _cpu_guarded_write
    xdef _cpu68k_as_read_byte
    xdef _cpu68k_as_write_byte
    xdef _cpu_sleep
    xdef _cpu_halt
    xdef _cpu_enable_branch_cache
    xdef _cpu060_set_pcr_bits
    xdef _usp_get
    xdef _usp_set
    xdef _usp_grow
    xdef _usp_shrink


;-------------------------------------------------------------------------------
; int cpu_get_model(void)
; Returns the CPU model identifier.
_cpu_get_model:
    inline
        movem.l d3 / a2 - a3, -(sp)
        moveq   #CPU_MODEL_68060, d0    ; assume a 68060 by default
        move.l  #16, a0     ; save the default illegal instruction handler
        move.l  (a0), -(sp)
        move.l  #44, a2     ; save the default line 1111 emulator handler
        move.l  (a2), -(sp)

        ; check for 68010
        lea     .is_68000_cpu(pc), a1
        move.l  a1, (a0)
        move.l  sp, a3      ; save the stack pointer for the exception handler
        dc.l    $4e7a3801   ; movec vbr, d3 (requires 68010+)

        ; check for 68020
        lea     .is_68010_cpu(pc), a1
        move.l  a1, (a0)
        move.l  sp, a3
        dc.l    $4e7a1002   ; movec cacr, d1 (requires 68020+)

        ; check for 68040
        lea     .is_68020_or_68030_cpu(pc), a1
        move.l  a1, (a0)
        move.l  sp, a3
        dc.l    $4e7a1004   ; movec itt0, d1 (requires 68040+)

        ; check for 68060
        lea     .is_68040_cpu(pc), a1
        move.l  a1, (a0)
        move.l  sp, a3
        dc.l    $4e7a1808   ; movec pcr, d1 (requires 68060+)

.done:
        move.l  (sp)+, a2   ; restore the default line 1111 emulator handler
        move.l  #44, a1
        move.l  a2, (a1)
        move.l  (sp)+, a0   ; restore the default illegal instruction handler
        move.l  #16, a1
        move.l  a0, (a1)
        movem.l (sp)+, d3 / a2 - a3
        rts

.is_68000_cpu:
        move.l  a3, sp      ; restore the stack pointer
        moveq   #CPU_MODEL_68000, d0
        bra     .done

.is_68010_cpu:
        move.l  a3, sp
        moveq   #CPU_MODEL_68010, d0
        bra     .done

.is_68020_cpu:
        move.l  a3, sp
        moveq   #CPU_MODEL_68020, d0
        bra     .done

.is_68020_or_68030_cpu:
        move.l  a3, sp

        ; check for 68030
        moveq   #CPU_MODEL_68030, d0
        lea     .is_68020_cpu(pc), a1
        move.l  a1, (a2)
        move.l  sp, a3
        dc.w    $f02f, $6200, $fffe ; pmove.w psr,-2(sp) (requires 68030+)
        move.l  a3, sp
        bra     .done

.is_68040_cpu:
        move.l  a3, sp
        moveq   #CPU_MODEL_68040, d0
        bra     .done
    einline


;-------------------------------------------------------------------------------
; void pop_exception_stack_frame(void)
; Pops the bus error stack frame that is sitting at sp from the stack. Call this
; function as a subroutine with ssp pointing to the bottom of the exception
; stack frame right before you call this function.
; Trashes: d0, a0
_pop_exception_stack_frame:
    inline
        ; pop off the return address and save it in a0
        move.l  (sp)+, a0

        ; get the exception stack frame format id
        ;moveq.l #0, d0
        move.b  6(sp), d0
        lsr.b   #4, d0

        ; pop off the right number of bytes based on the exception stack frame id
        cmp.b   #$0, d0
        bne.s   .dismiss_non_format_0_exception_stack_frame
        addq.l  #8, sp
        jmp     (a0)

.dismiss_non_format_0_exception_stack_frame:
        cmp.b   #$2, d0
        bne.s   .dismiss_non_format_2_exception_stack_frame
        add.l   #12, sp
        jmp     (a0)

.dismiss_non_format_2_exception_stack_frame:
        cmp.b   #$3, d0
        bne.s   .dismiss_non_format_3_exception_stack_frame
        add.l   #12, sp
        jmp     (a0)

.dismiss_non_format_3_exception_stack_frame:
        cmp.b   #$4, d0
        bne.s   .dismiss_non_format_4_exception_stack_frame
        add.l   #12, sp
        jmp     (a0)

.dismiss_non_format_4_exception_stack_frame:
        cmp.b   #$7, d0
        bne.s   .dismiss_non_format_7_exception_stack_frame
        add.l   #60, sp
        jmp     (a0)

.dismiss_non_format_7_exception_stack_frame:
        cmp.b   #$9, d0
        bne.s   .dismiss_non_format_9_exception_stack_frame
        add.l   #20, sp
        jmp     (a0)

.dismiss_non_format_9_exception_stack_frame:
        cmp.b   #$a, d0
        bne.s   .dismiss_non_format_a_exception_stack_frame
        add.l   #32, sp
        jmp     (a0)

.dismiss_non_format_a_exception_stack_frame:
        cmp.b   #$b, d0
        bne.s   .dismiss_non_format_b_exception_stack_frame
        add.l   #92, sp
        jmp     (a0)

.dismiss_non_format_b_exception_stack_frame:
        move.l  #RGB_YELLOW, -(sp)
        jmp     _cpu_non_recoverable_error
        ; NOT REACHED
    einline


;-------------------------------------------------------------------------------
; int cpu_verify_ram_4b(void* pSrc)
; Verifies that the 4 bytes starting at the RAM location 'pSrc' can be
; successfully read and written without bit corruption. Catches bus errors and
; returns -1 if a bus error or corrupted data is detected and 0 on success.
; Note that this function does not preserve the original values of the memory
; locations it writes to. Note that this function expects that 'pSrc' is long
; word aligned.
_cpu_verify_ram_4b:
    inline
    cargs   cvr4b_src_ptr.l

        move.l  cvr4b_src_ptr(sp), a0
        movem.l a2 - a3, -(sp)

        ; compute the address of the bus error vector and park it in a3
        movec   vbr, a3
        addq.l  #8, a3

        ; install our own bus error handler
        move.l  (a3), -(sp)
        lea     .mem_bus_error_handler(pc), a2
        move.l  a2, (a3)

        ; verify the bytes
        move.l  #$55555555, d0

        move.l  d0, (a0)
        move.l  (a0), d1
        cmp.l   d0, d1
        bne.s   .failed

        move.l  #$aaaaaaaa, d0

        move.l  d0, (a0)
        move.l  (a0), d1
        cmp.l   d0, d1
        bne.s   .failed

        ; success
        moveq.l #0, d0
.done:
        move.l  (sp)+, (a3)
        movem.l (sp)+, a2 - a3
        rts

.mem_bus_error_handler:
        ; pop the exception stack frame off the stack
        jsr     _pop_exception_stack_frame(pc)

.failed:
        moveq.l #-1, d0
        bra.s   .done
    einline


;-------------------------------------------------------------------------------
; int cpu_guarded_read(void* src, void* buffer, int buffer_size)
; Reads the bytes starting at 'src' and writes them to 'buffer'. 'buffer_size'
; bytes are read and copied. Catches bus errors and returns -1 in such an event.
; Returns 0 if all bytes have been successfully read. Must be called from
; supervisor mode.
_cpu_guarded_read:
    inline
    cargs   mgr_src.l, mgr_buffer.l, mgr_buffer_size.l

        move.l  mgr_src(sp), a0
        move.l  mgr_buffer(sp), a1
        move.l  mgr_buffer_size(sp), d0
        movem.l a2 - a3, -(sp)

        ; compute the address of the bus error vector and park it in a3
        movec   vbr, a3
        addq.l  #8, a3

        ; install our special bus error handler
        move.l  (a3), -(sp)
        lea     .mem_bus_error_handler(pc), a2
        move.l  a2, (a3)

        ; Read the bytes
        beq     .success   ; nothing to do if buffer_size == 0
        subq.l  #1, d0
.L1:
        move.b  (a0)+, (a1)+
        dbra    d0, .L1

.success:
        moveq   #0, d0
.done:
        move.l  (sp)+, (a3)
        movem.l (sp)+, a2 - a3
        rts

.mem_bus_error_handler:
        ; pop the exception stack frame off the stack
        jsr     _pop_exception_stack_frame(pc)
        moveq   #-1, d0
        bra.s   .done
    einline


;-------------------------------------------------------------------------------
; int cpu_guarded_write(void* dst, void* buffer, int buffer_size)
; Writes the bytes starting at 'buffer' to 'dst'. 'buffer_size' bytes are written.
; Catches bus errors and returns -1 in such an event. Returns 0 if all bytes have
; been successfully written. Must be called from supervisor mode.
_cpu_guarded_write:
    inline
    cargs   mgw_dst.l, mgw_buffer.l, mgw_buffer_size.l

        move.l  mgw_dst(sp), a0
        move.l  mgw_buffer(sp), a1
        move.l  mgw_buffer_size(sp), d0
        movem.l a2 - a3, -(sp)

        ; compute the address of the bus error vector and park it in a3
        movec   vbr, a3
        addq.l  #8, a3

        ; install our special bus error handler
        move.l  8(a3), -(sp)
        lea     .mem_bus_error_handler(pc), a2
        move.l  a2, (a3)

        ; Write the bytes
        beq     .success   ; nothing to do if buffer_size == 0
        sub.l   #1, d0
.L1:
        move.b  (a1)+, (a0)+
        dbra    d0, .L1

.success:
        moveq   #0, d0
.done:
        move.l  (sp)+, (a3)
        movem.l (sp)+, a2 - a3
        rts

.mem_bus_error_handler:
        ; pop the exception stack frame off the stack
        jsr     _pop_exception_stack_frame(pc)
        moveq   #-1, d0
        bra.s   .done
    einline


;-------------------------------------------------------------------------------
; unsigned int cpu68k_as_read_byte(void* p, int addr_space)
; Reads a byte from address 'p' in the address space 'addr_space'.
_cpu68k_as_read_byte:
    inline
    cargs   arb_p.l, arb_addr_space.l

        DISABLE_INTERRUPTS d1
        move.l  arb_p(sp), a0
        move.l  arb_addr_space(sp), d0
        movec   d0, sfc
        moveq   #0, d0
        moves.b (a0), d0
        RESTORE_INTERRUPTS d1
        rts
    einline


;-------------------------------------------------------------------------------
; void cpu68k_as_write_byte(void* p, int addr_space, unsigned int val)
; Writes the byte 'val' to address 'p' in the address space 'addr_space'.
_cpu68k_as_write_byte:
    inline
    cargs   awb_p.l, awb_addr_space.l, awb_val.l

        DISABLE_INTERRUPTS d1
        move.l  awb_p(sp), a0
        move.l  awb_addr_space(sp), d0
        movec   d0, dfc
        move.l  awb_val(sp), d0
        moves.b d0, (a0)
        RESTORE_INTERRUPTS d1
        rts
    einline


;-------------------------------------------------------------------------------
; void cpu_sleep(int cpu_type)
; Moves the CPU to (a low power) sleep state until an interrupt occurs.
_cpu_sleep:
    inline
    cargs cslp_cpu_type.l
        cmp.l   #CPU_MODEL_68060, cslp_cpu_type(sp)
        beq.s   .cpu_sleep_68060
        stop    #$2000
        rts

.cpu_sleep_68060:
        lpstop  #$2000
        rts
    einline


;-------------------------------------------------------------------------------
; void cpu_halt(void)
; Turns interrupt recognition off and halts the CPU in an endless loop.
_cpu_halt:
    inline
        stop    #$2700
        bra.s   _cpu_halt
    einline


;-------------------------------------------------------------------------------
; void cpu_enable_branch_cache(int flag)
; Enables/disables the branch cache on the 68060.
; NOTE: assumes IRQs are off on call
_cpu_enable_branch_cache:
    inline
    cargs cebc_flag.l
        move.l  cebc_flag(sp), d0
        beq.s   .disable_branch_cache
        movec   cacr, d0
        bset    #CACR_CABC_BIT, d0
        movec   d0, cacr        ; clear all cache entries
        movec   cacr, d0
        bset    #CACR_EBC_BIT, d0
        movec   d0, cacr        ; enable branch cache
        rts

.disable_branch_cache:
        movec   cacr, d0
        bclr    #CACR_EBC_BIT, d0
        movec   d0, cacr
        rts
    einline


;-------------------------------------------------------------------------------
; void cpu060_set_pcr_bits(uint32_t bits)
; Sets bits in the 68060 PCR.
_cpu060_set_pcr_bits:
    inline
    cargs cspb_bits.l
        move.l  cspb_bits(sp), d0
        movec   pcr, d1
        or.l    d0, d1
        movec   d1, pcr
        rts
    einline


;-------------------------------------------------------------------------------
; uintptr_t usp_get(void)
; Returns the current user stack pointer.
_usp_get:
        move.l  usp, a0
        move.l  a0, d0
        rts


;-------------------------------------------------------------------------------
; void usp_set(uintptr_t sp)
; Updates the current user stack pointer to 'sp'.
_usp_set:
    inline
    cargs usps_pc.l
        move.l  usps_pc(sp), a0
        move.l  a0, usp
        rts
    einline


;-------------------------------------------------------------------------------
; uintptr_t usp_grow(size_t nbytes)
; Grows the current user stack by 'pushing' 'nbytes' on it. Returns the new sp.
_usp_grow:
    inline
    cargs usp_gr_nbytes.l
        move.l  usp_gr_nbytes(sp), d0
        move.l  usp, a0
        sub.l   d0, a0
        move.l  a0, usp
        move.l  a0, d0
        rts
    einline


;-------------------------------------------------------------------------------
; void usp_shrink(size_t nbytes)
; Shrinks the current user stack by 'popping off' 'nbytes'.
_usp_shrink:
    inline
    cargs usp_sh_nbytes.l
        move.l  usp_sh_nbytes(sp), d0
        move.l  usp, a0
        add.l   d0, a0
        move.l  a0, usp
        rts
    einline
