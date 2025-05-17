;
;  syscall.i
;  libsystem
;
;  Created by Dietmar Planitzer on 9/6/23.
;  Copyright © 2023 Dietmar Planitzer. All rights reserved.
;

        ifnd __SYSCALL_I
__SYSCALL_I  set 1

SC_read                     equ 0
SC_write                    equ 1
SC_clock_wait               equ 2
SC_disp_schedule            equ 3
SC_vmalloc                  equ 4
SC_exit                     equ 5
SC_spawn                    equ 6
SC_getpid                   equ 7
SC_getppid                  equ 8
SC_getpargs                 equ 9
SC_open                     equ 10
SC_close                    equ 11
SC_waitpid                  equ 12
SC_lseek                    equ 13
SC_getcwd                   equ 14
SC_chdir                    equ 15
SC_getuid                   equ 16
SC_getumask                 equ 17
SC_setumask                 equ 18
SC_mkdir                    equ 19
SC_getinfo                  equ 20
SC_opendir                  equ 21
SC_setinfo                  equ 22
SC_access                   equ 23
SC_fgetinfo                 equ 24
SC_fsetinfo                 equ 25
SC_unlink                   equ 26
SC_rename                   equ 27
SC_ioctl                    equ 28
SC_truncate                 equ 29
SC_ftruncate                equ 30
SC_creat                    equ 31
SC_pipe                     equ 32
SC_disp_timer               equ 33
SC_disp_create              equ 34
SC_disp_getcurrent          equ 35
SC_dispose                  equ 36
SC_clock_gettime            equ 37
SC_lock_create              equ 38
SC_lock_trylock             equ 39
SC_lock_lock                equ 40
SC_lock_unlock              equ 41
SC_sem_create               equ 42
SC_sem_post                 equ 43
SC_sem_wait                 equ 44
SC_sem_trywait              equ 45
SC_cond_create              equ 46
SC_cond_wake                equ 47
SC_cond_timedwait           equ 48
SC_disp_removebytag         equ 49
SC_mount                    equ 50
SC_unmount                  equ 51
SC_getgid                   equ 52
SC_sync                     equ 53
SC_coninit                  equ 54
SC_fsgetdisk                equ 55
SC_vcpuerrno                equ 56
SC_chown                    equ 57

        endif   ; __SYSCALL_I
