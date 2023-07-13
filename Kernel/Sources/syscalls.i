;
;  syscalls.i
;  Apollo
;
;  Created by Dietmar Planitzer on 4/4/21.
;  Copyright © 2021 Dietmar Planitzer. All rights reserved.
;

    ifnd SYSCALLS_I
SYSCALLS_I    set 1

; Error codes
EOK         equ 0
ENOMEM      equ 1
ENODATA     equ 2
ENOTCDF     equ 3
ENOBOOT     equ 4
ENODRIVE    equ 5
EDISKCHANGE equ 6
ETIMEOUT    equ 7
ENODEVICE   equ 8
EPARAM      equ 9
ERANGE      equ 10


; System call numbers
SC_sleep            equ     0       ; sleep(seconds: d1, nanoseconds: d2) -> Void
SC_print            equ     1       ; print(pString: a1) -> Void
SC_dispatchAsync    equ     2       ; dispatchAsync(pUserClosure: a1) -> Void
SC_numberOfCalls    equ     3       ; number of system calls


; System call ABI
;
; d0 - d1 / a0 - a1 -> volatile registers (NOT preserved across syscall)
; d2 - d7 / a2 - a6 -> non-volatile (preserved across syscall)
;
; trap #0
; -> d0:        system call number
; -> d1...d7:   first 7 integer parameters in order from left to right. Extra parameters are passed on the stack from left to right
; -> a1...a5:   first 5 pointer parameters in order from left to right. Extra parameters are passed on the stack from left to right
; -> fp0...fp7: ditto for floating point values
;
; <- d0:        0 if sucdess; > 0 if an error occured with d0 holding the error number
; <- d1:        32bit integer return value
; <- d1/d2:     64bit integer return value
; <- a0:        32bit pointer return value
; <- fp0:       96bit floating point return value

    ; Invoke the system call 'num'
    macro SYSCALL num
    move.w  #\1, d0
    trap #0
    endm

    endif
