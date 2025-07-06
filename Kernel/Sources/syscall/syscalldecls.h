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
#include <dispatcher/VirtualProcessor.h>
#include <process/ProcessPriv.h>


typedef intptr_t (*syscall_func_t)(void* _Nonnull p, void* _Nonnull args);

typedef struct syscall {
    syscall_func_t  f;
    intptr_t        flags;
} syscall_t;

#define SC_ERRNO    1   /* System call returns an error that should be stored in vcpu->errno */
#define SC_VCPU     2   /* System call expects a vcpu_t* rather than a proc_t* */


#define SYSCALL_REF(__name) \
extern intptr_t _SYSCALL_##__name(void* p, void* args)


#define SYSCALL_ENTRY(__name, __flags) \
{(syscall_func_t)_SYSCALL_##__name, __flags}


#define SYSCALL_0(__name) \
struct args##__name { \
    unsigned int scno; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_1(__name, __p1) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_2(__name, __p1, __p2) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_3(__name, __p1, __p2, __p3) \
struct args##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args##__name* _Nonnull pa)

#define SYSCALL_4(__name, __p1, __p2, __p3, __p4) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)

#define SYSCALL_5(__name, __p1, __p2, __p3, __p4, __p5) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)

#define SYSCALL_6(__name, __p1, __p2, __p3, __p4, __p5, __p6) \
struct args_##__name { \
    unsigned int scno; \
    __p1; \
    __p2; \
    __p3; \
    __p4; \
    __p5; \
    __p6; \
}; \
intptr_t _SYSCALL_##__name(void* _Nonnull p, const struct args_##__name* _Nonnull pa)


#endif /* _SYSCALLDECLS_H_ */
