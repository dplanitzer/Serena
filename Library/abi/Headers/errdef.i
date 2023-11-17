;
;  errdef.i
;  Apollo
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd _ERRDEF_I
_ERRDEF_I  set 1

EOK         equ 0
ENOMEM      equ 1
ENOMEDIUM   equ 2
EDISKCHANGE equ 3
ETIMEDOUT   equ 4
ENODEV      equ 5
EPARAM      equ 6
ERANGE      equ 7
EINTR       equ 8
EAGAIN      equ 9
EPIPE       equ 10
EBUSY       equ 11
ENOSYS      equ 12
EINVAL      equ 13
EIO         equ 14
EPERM       equ 15
EDEADLK     equ 16
EDOM        equ 17
ENOEXEC     equ 18
E2BIG       equ 19
ENOENT      equ 20
ENOTBLK     equ 21
EBADF       equ 22
ECHILD      equ 23
ESRCH       equ 24
ESPIPE      equ 25
ENOTDIR     equ 26
ENAMETOOLONG    equ 27
EACCESS     equ 28


__EFIRST    equ 1
__ELAST     equ 28

        endif   ; _ERRDEF_I
