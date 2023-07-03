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

    xdef _cpu_enable_irqs
    xdef _cpu_disable_irqs
    xdef _cpu_restore_irqs
    xdef _cpu_get_model
    xdef _cpu_guarded_read
    xdef _cpu_guarded_write
    xdef _cpu_sleep
    xdef _cpu_call_as_user
    xdef _cpu_return_from_call_as_user
    xdef _cpu_push_async_user_closure_invocation
    xdef _cpu_async_user_closure_trampoline
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
; Int cpu_get_bus_error_frame_size(frame_ptr[a0.l], cpu_type[d0.b])
; Returns the size of the bus error stack frame which starts at 'frame_ptr'. The
; CPU type is given by 'cpu_type'.
_cpu_get_bus_error_frame_size:
    inline
    ; 68000   7 words                                        (68000UM page 6-17)
    ; 68010  29 words (format $8)                            (68000UM page 6-18)
    ; 68020  16 words (format $a)  or  46 words (format $b)  (68020UM page 6-28)
    ; 68030  16 words (format $a)  or  46 words (format $b)  (68030UM page 8-34)
    ; 68040  30 words (format $7)
    ; 68060   8 words (format $4)                            (UM68060 page 8-21)
        cmp.b   #0, d0
        beq     .is_68000
        cmp.b   #1, d0
        beq     .is_68010
        cmp.b   #2, d0
        beq     .is_68020_or_68030
        cmp.b   #3, d0
        beq     .is_68020_or_68030
        cmp.b   #4, d0
        beq     .is_68040
        cmp.b   #6, d0
        beq     .is_68060
        moveq   #0, d0
        rts

.is_68000:
        moveq   #2*7, d0
        rts

.is_68010:
        moveq   #2*29, d0
        rts

.is_68020_or_68030:
        move.w  6(a0), d0
        lsr.w   #8, d0
        lsr.w   #4, d0
        cmp.w   #$a, d0
        bne     .is_68020_or_68030_long_format
        moveq   #2*16, d0
        rts

.is_68020_or_68030_long_format:
        moveq   #2*46, d0
        rts

.is_68040:
        moveq   #2*30, d0
        rts

.is_68060:
        moveq   #2*8, d0
        rts
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
        lea     8, a3

        ; install our special bus error handler
        move.l  (a3), -(sp)
        lea     .mem_bus_error_handler(pc), a2
        move.l  a2, (a3)

        ; Read the bytes
        beq     .success   ; nothing to do if buffer_size == 0
        sub.l   #1, d0
.L1:
        move.b  (a0)+, (a1)+
        dbra    d0, .L1

.success:
        moveq   #0, d0
        bra     .done
.failed:
        moveq   #-1, d0
.done:
        move.l  (sp)+, (a3)
        movem.l (sp)+, a2 - a3
        rts

.mem_bus_error_handler:
        ; clear the stack frame that the CPU put on the stack. Note that the size
        ; of the frame that the CPU wrote to the stack depends on the CPU type.
        moveq   #0, d0
        move.b  (SYS_DESC_BASE + sd_cpu_model), d0
        move.l  sp, a0
        jsr _cpu_get_bus_error_frame_size
        add.l   d0, sp
        bra     .failed
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
        lea     8, a3

        ; install our special bus error handler
        move.l  (a3), -(sp)
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
        bra     .done
.failed:
        moveq   #-1, d0
.done:
        move.l  (sp)+, (a3)
        movem.l (sp)+, a2 - a3
        rts

.mem_bus_error_handler:
        ; clear the stack frame that the CPU put on the stack. Note that the size
        ; of the frame that the CPU wrote to the stack depends on the CPU type.
        moveq   #0, d0
        move.b  (SYS_DESC_BASE + sd_cpu_model), d0
        move.l  sp, a0
        jsr _cpu_get_bus_error_frame_size
        add.l   d0, sp
        bra     .failed
    einline


;-------------------------------------------------------------------------------
; void cpu_sleep(void)
; Moves the CPU to (a low power) sleep state until an interrupt occurs.
_cpu_sleep:
    cmp.b   #CPU_MODEL_68060, SYS_DESC_BASE + sd_cpu_model
    beq.s   .cpu_sleep_68060
    stop    #$2000
    rts

.cpu_sleep_68060:
    ; the 68060 emulator doesn't support lpstop...
    ;lpstop  #$2000
    stop    #$2000
    rts


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
; void cpu_push_async_user_closure_invocation(UInt options, UInt32* _Nonnull usp, UInt32 pReturnAddress, Cpu_UserClosure _Nonnull pClosure, Byte* _Nullable pContext)
; Pushes an AUCI (asynchonous user closure invocation) frame on the given user stack.
; 'pClosure' will be invoked with 'pContext' in user space the next time the
; VP which owns the user stack is scheduled for execution. This function must be
; called while the VP is suspended. This call can be used to inject a call to
; some user space function into the normal flow of the VP. The AUCI will return
; to the previous flow when done. The AUCI call is completely transparent to the
; surrounding user space code.
;
; NOTE: this function assumes that the USP is correctly aligned for a return address (2 byte alignment)
CPU_AUCI_SAVE_FP_STATE_BIT  equ 0
_cpu_push_async_user_closure_invocation:
    cargs cpa_options.l, cpa_usp_ptr.l, cpa_return_address.l, cpa_closure_ptr.l, cpa_context_ptr.l

    ; The AUCI frame looks like this:
    ; 4 return adress
    ; 4 pClosure
    ; 4 pContext
    ; 2 trampoline control word
    move.l  cpa_usp_ptr(sp), a0
    move.l  (a0), a0
    move.l  cpa_return_address(sp), a1
    move.l  a1, -(a0)
    move.l  cpa_closure_ptr(sp), a1
    move.l  a1, -(a0)
    move.l  cpa_context_ptr(sp), a1
    move.l  a1, -(a0)
    move.l  cpa_options(sp), d0
    move.w  d0, -(a0)

    move.l  cpa_usp_ptr(sp), a1
    move.l  a0, (a1)

    rts


;-----------------------------------------------------------------------
; void cpu_async_user_closure_trampoline(void)
; This function implements the transparent invocation of the asynchronous user
; closure. The VP PC must be set to point to this function and the cpu_push_auci()
; function must have been called to push the required AUCI frame on the user stack.
; This function removes the frame that was pushed by cpu_push_auci()
;
; Note that this code runs in user space.
_cpu_async_user_closure_trampoline:
    move        ccr, -(sp)
    movem.l     d0 - d7 / a0 - a6, -(sp)

    move.w      (15 * 4 + 2 + 0 + 0)(sp), d7
    btst        #CPU_AUCI_SAVE_FP_STATE_BIT, d7
    beq.s       .Lno_fp_save
    fmovem      fp0 - fp7, -(sp)
    fmovem.l    fpcr, -(sp)
    fmovem.l    fpsr, -(sp)
    fmovem.l    fpiar, -(sp)
    move.l      (15 * 4 + 2 + 2 + 8 * 12 + 3 * 4 + 0)(sp), a1
    move.l      (15 * 4 + 2 + 2 + 8 * 12 + 3 * 4 + 4)(sp), a0
    bra.s       .Lfp_save_done

.Lno_fp_save:
    move.l      (15 * 4 + 2 + 2 + 0)(sp), a1
    move.l      (15 * 4 + 2 + 2 + 4)(sp), a0

.Lfp_save_done:
    move.l      a1, -(sp)
    ; user stack frame at this point:
    ; 4  return address
    ; 4  pClosure  (a0)
    ; 4  pContext  (a1)
    ; 2  trampoline control word (d0)
    ; 2  CCR
    ; 4* d0 - d7 / a0 - a6
    ; 4* fp0 - fp7, fpcr, fpsr, fpiar
    ; 4  pContext
    jsr         (a0)

    addq.l      #4, sp
    btst        #CPU_AUCI_SAVE_FP_STATE_BIT, d7
    beq.s       .Lfp_restore_done
    fmovem.l    (sp)+, fpiar
    fmovem.l    (sp)+, fpsr
    fmovem.l    (sp)+, fpcr
    fmovem      (sp)+, fp0 - fp7

.Lfp_restore_done:
    movem.l     (sp)+, d0 - d7 / a0 - a6
    move        (sp)+, ccr
    adda        #10, sp      ; note that this instruction does not update the CCR
    rts

