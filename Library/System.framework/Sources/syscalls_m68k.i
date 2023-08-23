;
;  syscalls.i
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    ifnd SYSCALLS_I
SYSCALLS_I    set 1

; System call numbers
SC_write                equ 0       ; write(pString: d1.l)
SC_sleep                equ 1       ; sleep(seconds: d1.l, nanoseconds: d2.l)
SC_dispatch_async       equ 2       ; dispatch_async(pUserClosure: d1.l)
SC_alloc_address_space  equ 3       ; alloc_address_space(nbytes: d1.l, pOutMem: d1.l)
SC_exit                 equ 4       ; exit(status: d1.l) [noreturn]
SC_spawn_process        equ 5       ; spawn_process(pUserEntryPoint: d1.l)


; System call ABI
;
; 
; d1 - d7 / a0 - a7 -> non-volatile (register contents is preserved across syscall)
;
; The reason for preserving all CPU registers is that the kernel does not want to
; accidently leak superuser only information to user space.
;
; trap #0
; -> d0:        system call number
; -> d1...d7:   up to 7 integer/pointer parameters in the order of left-most to right-most argument
;
; <- d0:        0 if success; > 0 if an error occured with d0 holding the error number (always returned)
; <- d1:        32bit integer or pointer return value
; <- d1/d2:     64bit integer return value

    ; Invoke the system call 'num'
    macro SYSCALL num
    move.w  #\1, d0
    trap #0
    endm

    endif
