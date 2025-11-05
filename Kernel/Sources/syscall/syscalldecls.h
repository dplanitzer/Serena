//
//  syscalldecls.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYSCALLDECLS_H_
#define _SYSCALLDECLS_H_

#include <kern/types.h>
#include <process/ProcessPriv.h>
#include <sched/vcpu.h>


typedef intptr_t (*syscall_func_t)(vcpu_t _Nonnull vp, void* _Nonnull args);

typedef struct syscall_entry {
    syscall_func_t  f;
    char            ret_type;   // return type
} syscall_entry_t;


#define SC_INT      0       /* Simple int return*/
#define SC_ERRNO    1       /* System call returns an error that should be stored in vcpu->errno */
#define SC_PTR      2       /* System call returns a pointer */
#define SC_VOID     3       /* System call returns nothing. Eg sigurgent() */
#define SC_NORETURN SC_VOID /* System call does not return */


#define SYSCALL_REF(__name) \
extern intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, void* _Nonnull args)


#define SYSCALL_ENTRY(__name, __flags) \
{(syscall_func_t)_SYSCALL_##__name, __flags}


typedef struct syscall_args {
    unsigned int dummy;
    unsigned int scno;
} syscall_args_t;


#define SYSCALL_0(__name) \
struct args##__name { \
    syscall_args_t  h; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args##__name* _Nonnull pa)

#define SYSCALL_1(__name, __p1) \
struct args##__name { \
    syscall_args_t  h; \
    __p1; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args##__name* _Nonnull pa)

#define SYSCALL_2(__name, __p1, __p2) \
struct args##__name { \
    syscall_args_t  h; \
    __p1; \
    __p2; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args##__name* _Nonnull pa)

#define SYSCALL_3(__name, __p1, __p2, __p3) \
struct args##__name { \
    syscall_args_t  h; \
    __p1; \
    __p2; \
    __p3; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args##__name* _Nonnull pa)

#define SYSCALL_4(__name, __p1, __p2, __p3, __p4) \
struct args_##__name { \
    syscall_args_t  h; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args_##__name* _Nonnull pa)

#define SYSCALL_5(__name, __p1, __p2, __p3, __p4, __p5) \
struct args_##__name { \
    syscall_args_t  h; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args_##__name* _Nonnull pa)

#define SYSCALL_6(__name, __p1, __p2, __p3, __p4, __p5, __p6) \
struct args_##__name { \
    syscall_args_t  h; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
    __p6; \
}; \
intptr_t _SYSCALL_##__name(vcpu_t _Nonnull vp, const struct args_##__name* _Nonnull pa)


#endif /* _SYSCALLDECLS_H_ */
