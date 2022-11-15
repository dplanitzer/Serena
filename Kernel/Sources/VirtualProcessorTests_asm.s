;
;  VirtualProcessorTests_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/19/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    include "syscalls.i"

    xdef _OnUserSpaceCallTest
    xdef _OnAsyncUserSpaceCallTest_RunningInKernelSpace
    xdef _OnAsyncUserSpaceCallTest_RunningInUserSpace
    xdef _OnInjectedHelloWorld


;-------------------------------------------------------------------------------
; void OnUserSpaceCallTest(Byte* _Nullable pContext)
_OnUserSpaceCallTest:
    cargs usct_context_ptr.l
    lea     .msg, a1
    SYSCALL SC_PRINT
    move.l  #10, d1
    move.l  #250*1000*1000, d2
    SYSCALL SC_SLEEP
    rts

.msg:
    dc.b    "Hello World from user space!", 10, 0
    align   2


;-------------------------------------------------------------------------------
; void OnAsyncUserSpaceCallTest_RunningInKernelSpace(Byte* _Nullable pContext)
; Code which is guaranteed to run in kernel space inside a system call
_OnAsyncUserSpaceCallTest_RunningInKernelSpace:
    cargs ausct2_context_ptr.l
    lea     .msg, a1
    SYSCALL SC_PRINT
    move.l  #1000, d1
    move.l  #0, d2
    SYSCALL SC_SLEEP
    rts

.msg:
    dc.b    "Hello World from user space!", 10, 0
    align   2

;-------------------------------------------------------------------------------
; void OnAsyncUserSpaceCallTest_RunningInUserSpace(Byte* _Nullable pContext)
; Code which runs purely in user space
_OnAsyncUserSpaceCallTest_RunningInUserSpace:
    cargs ausct_context_ptr.l
.L1:
    bra.s   .L1
    rts


;-------------------------------------------------------------------------------
; void OnInjectedHelloWorld(Byte* _Nullable pContext)
_OnInjectedHelloWorld:
    cargs iusct_context_ptr.l
    lea     .msg, a1
    SYSCALL SC_PRINT
    rts

.msg:
    dc.b    "Hello World from injected user space!", 10, 0
    align   2
