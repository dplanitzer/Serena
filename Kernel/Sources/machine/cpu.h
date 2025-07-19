//
//  cpu.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CPU_H
#define _CPU_H 1

#include <stdnoreturn.h>

#ifdef __M68K__
#include <machine/arch/m68k/cpu.h>
#else
#error "don't know how to align stack pointers"
#endif


typedef void (*Cpu_UserClosure)(void* _Nullable context, void* _Nullable arg);


extern const char* _Nonnull cpu_get_model_name(int8_t cpu_model);

extern int cpu_verify_ram_4b(void* pSrc);

extern int cpu_guarded_read(void* _Nonnull src, void* _Nonnull buffer, int buffer_size);
extern int cpu_guarded_write(void* _Nonnull dst, const void* _Nonnull buffer, int buffer_size);

extern unsigned int cpu68k_as_read_byte(void* p, int addr_space);
extern void cpu68k_as_write_byte(void* p, int addr_space, unsigned int val);

extern void cpu_sleep(int cpu_type);
extern void cpu_halt(void);

extern void cpu_make_callout(CpuContext* _Nonnull cp, void* _Nonnull ksp, void* _Nonnull usp, bool isUser, VoidFunc_1 _Nonnull func, void* _Nullable arg, VoidFunc_0 _Nonnull ret_func);

extern void cpu_relinquish_from_user(void);

extern _Noreturn cpu_non_recoverable_error(void);
extern _Noreturn mem_non_recoverable_error(void);

extern const char* _Nonnull fpu_get_model_name(int8_t fpu_model);

#endif /* _CPU_H */
