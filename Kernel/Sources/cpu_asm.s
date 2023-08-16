;
;  cpu_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 2/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "chipset.i"
    include "lowmem.i"
    include "syscalls.i"

    xref _cpu_non_recoverable_error

    xdef _cpu_enable_irqs
    xdef _cpu_disable_irqs
    xdef _cpu_restore_irqs
    xdef _cpu_set_irq_stack_pointer
    xdef _cpu_get_model
    xdef _cpu_guarded_read
    xdef _cpu_guarded_write
    xdef _cpu_sleep
    xdef _cpu_call_as_user
    xdef _cpu_abort_call_as_user
    xdef _cpu_return_from_call_as_user
    xdef _fpu_get_model



;-------------------------------------------------------------------------------
; void cpu_enable_irqs(void)
; Enables interrupt handling.
_cpu_enable_irqs:
    and.w   #$f8ff, sr
    rts

;-------------------------------------------------------------------------------
; Int cpu_disable_irqs(void)
; Disables interrupt handling and returns the previous interrupt handling state.
; Returns the old IRQ state
_cpu_disable_irqs:
    DISABLE_INTERRUPTS d0
    rts


;-------------------------------------------------------------------------------
; void cpu_restore_irqs(Int state)
; Restores the given interrupt handling state.
_cpu_restore_irqs:
    cargs cri_state.l
    move.l  cri_state(sp), d0
    RESTORE_INTERRUPTS d0
    rts


;-------------------------------------------------------------------------------
; Int cpu_get_model(void)
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
; fpu_get_model
; Returns the FPU model identifier
_fpu_get_model:
    inline
        movem.l a2 / d2, -(sp)
        move.l  #44, a0     ; install the line F handler
        move.l  (a0), -(sp)
        lea     .done(pc), a1
        move.l  a1, (a0)
        move.l  sp, a2     ; save the stack pointer

        moveq   #FPU_MODEL_NONE, d0
        dc.l    $f201583a  ; ftst.b d1 - force the fsave instruction to generate an IDLE frame
        dc.w    $f327      ; fsave -(a7) - save the IDLE frame to the stack
        move.l  a2, d2
        sub.l   sp, d2     ; find out how big the IDLE frame is
        moveq   #FPU_MODEL_68881, d0
        cmp.b   #$1c, d2   ; 68881 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68882, d0
        cmp.b   #$3c, d2   ; 68882 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68040, d0
        cmp.b   #$4, d2     ; 68040 IDLE frame size
        beq.s   .done
        moveq   #FPU_MODEL_68060, d0
        cmp.b   #$c, d2
        beq.s   .done
        moveq   #FPU_MODEL_NONE, d0 ; unknown FPU model

.done:
        move.l  a2, sp      ; restore the stack pointer
        move.l  (sp)+, (a0) ; restore the line F handler
        movem.l (sp)+, a2 / d2
        rts
    einline


;-------------------------------------------------------------------------------
; Void pop_bus_error_stack_frame(continuation_addr[a0.l])
; Pops the bus error stack frame that is sitting at sp from the stack. We decode
; the frame type to figure out how many words to remove from the stack. The
; function then jumps to 'continuation_addr' to continue execution.
; Trashes: d0
_pop_bus_error_stack_frame:
    inline
    ; 68000   7 words (NOT supported)                        (68000UM page 6-17)
    ; 68010  29 words (format $8)                            (68000UM page 6-18)
    ; 68020  16 words (format $a)  or  46 words (format $b)  (68020UM page 6-28)
    ; 68030  16 words (format $a)  or  46 words (format $b)  (68030UM page 8-34)
    ; 68040  30 words (format $7)                            (68040UM page 8-24)
    ; 68060   8 words (format $4)                            (UM68060 page 8-21)
        move.w  6(sp), d0
        lsr.w   #8, d0
        lsr.w   #4, d0

        cmp.b   #$4, d0
        beq     .is_format_4
        cmp.b   #$7, d0
        beq     .is_format_7
        cmp.b   #$8, d0
        beq     .is_format_8
        cmp.b   #$a, d0
        beq     .is_format_a
        cmp.b   #$b, d0
        beq     .is_format_b
        jmp     _cpu_non_recoverable_error
        ; NOT REACHED

.is_format_4:
        moveq   #2*8, d0
        bsr.s   .done

.is_format_7:
        moveq   #2*30, d0
        bsr.s   .done

.is_format_8:
        moveq   #2*29, d0
        bsr.s   .done

.is_format_a:
        moveq   #2*16, d0
        bsr.s   .done

.is_format_b:
        moveq   #2*46, d0

.done:
        add.l   d0, sp
        jmp     (a0)
    einline


;-------------------------------------------------------------------------------
; Int cpu_guarded_read(Byte* src, Byte* buffer, Int buffer_size)
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
        ; clear the stack frame that the CPU put on the stack. Note that the size
        ; of the frame that the CPU wrote to the stack depends on the CPU type.
        lea     .mem_bus_error_handler_continue(pc), a0
        jmp     _pop_bus_error_stack_frame(pc)
.mem_bus_error_handler_continue:
        moveq   #-1, d0
        bra.s   .done
    einline


;-------------------------------------------------------------------------------
; Int cpu_guarded_write(Byte* dst, Byte* buffer, Int buffer_size)
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
        ; clear the stack frame that the CPU put on the stack. Note that the size
        ; of the frame that the CPU wrote to the stack depends on the CPU type.
        lea     .mem_bus_error_handler_continue(pc), a0
        jmp     _pop_bus_error_stack_frame(pc)
.mem_bus_error_handler_continue:
        moveq   #-1, d0
        bra.s   .done
    einline


;-------------------------------------------------------------------------------
; void cpu_sleep(Int cpu_type)
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


;-----------------------------------------------------------------------
; void cpu_call_as_user(Cpu_UserClosure _Nonnull pClosure, Byte* _Nullable pContext)
; Invokes the given closure in user space. Preserves the kernel integer register
; state. Note however that this function does not preserve the floating point 
; register state.
_cpu_call_as_user:
    inline
    cargs cau_closure_ptr.l, cau_context_ptr.l
        move.l  cau_closure_ptr(sp), a0
        move.l  cau_context_ptr(sp), a1
        movem.l d2 - d7 / a2 - a6, -(sp)

        ; zero out all integer registers to ensure that we do not leak kernel
        ; state into user space. We do not need to zero a0 and a1 because they
        ; hold the closure and context pointers which the userspace knows about
        ; anyway.
        moveq.l #0, d0
        moveq.l #0, d1
        moveq.l #0, d2
        moveq.l #0, d3
        moveq.l #0, d4
        moveq.l #0, d5
        moveq.l #0, d6
        moveq.l #0, d7
        move.l  d0, a2
        move.l  d0, a3
        move.l  d0, a4
        move.l  d0, a5
        move.l  d0, a6

        ; switch to user mode by dropping bit #13 in the SR register
        and.w   #$dfff, sr
        
        ; we are now in user mode
        move.l  a1, -(sp)
        jsr     (a0)
        addq.l  #4, sp

        ; go back to superuser mode
        trap    #1
        ; NOT REACHED
        ; (the return happens through _cpu_return_from_call_as_user)
    einline


;-----------------------------------------------------------------------
; void cpu_return_from_call_as_user(void)
; Invoked from trap#1 to switch us back into superuser mode and to return from
; the cpu_call_as_user() call.
_cpu_return_from_call_as_user:
    inline
        ; dismiss the (8 byte, format 0) exception stack frame
        addq.l  #8, sp
        ; restore the saved registers
        movem.l (sp)+, d2 - d7 / a2 - a6

        ; return with a rts. The sp register now points to the return address
        ; from which _cpu_call_as_user was called
        rts
    einline


;-----------------------------------------------------------------------
; void cpu_abort_call_as_user(void)
; Aborts the currently active user space call that is running on the same virtual
; processor that executes cpu_abort_call_as_user. This is the CPU specific low-
; level half of the VirtualProcessor_AbortCallAsUser() function. Note that aborting
; a user space call leaves the user space stack in an indeterminate state since
; the stack is not unrolled.
_cpu_abort_call_as_user:
    inline
        trap    #1
        ; NOT REACHED
        ; (the return happens through _cpu_return_from_call_as_user)
    einline