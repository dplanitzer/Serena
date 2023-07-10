;
;  DispatchQueueTests_asm.s
;  Apollo
;
;  Created by Dietmar Planitzer on 7/2/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

    include "syscalls.i"

    xdef _OnUserSpaceHelloWorldAndSleep
    xdef _OnUserSpaceHelloWorld


;-------------------------------------------------------------------------------
; void OnUserSpaceHelloWorldAndSleep(Byte* _Nullable pContext)
_OnUserSpaceHelloWorldAndSleep:
    cargs usct_context_ptr.l
    lea     .msg, a1
    SYSCALL SC_PRINT
    move.l  #10, d1
    move.l  #250*1000*1000, d2
    SYSCALL SC_SLEEP
    rts

.msg:
    dc.b    "Hello World from user space! (old)", 10, 0
    align   2


;-------------------------------------------------------------------------------
; void OnUserSpaceHelloWorld(Byte* _Nullable pContext)
_OnUserSpaceHelloWorld:
    cargs iusct_context_ptr.l
    lea     .msg, a1
    SYSCALL SC_PRINT
    rts

.msg:
    dc.b    "Hello World from user space! (old)", 10, 0
    align   2
