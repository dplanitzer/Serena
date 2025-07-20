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
#error "don't know how to support this CPU architecture"
#endif


extern const char* _Nonnull cpu_get_model_name(int8_t cpu_model);
extern const char* _Nonnull fpu_get_model_name(int8_t fpu_model);

extern int cpu_verify_ram_4b(void* pSrc);

extern int cpu_guarded_read(void* _Nonnull src, void* _Nonnull buffer, int buffer_size);
extern int cpu_guarded_write(void* _Nonnull dst, const void* _Nonnull buffer, int buffer_size);

extern void cpu_sleep(int cpu_type);
extern void cpu_halt(void);

extern void cpu_make_callout(mcontext_t* _Nonnull cp, void* _Nonnull ksp, void* _Nonnull usp, bool isUser, VoidFunc_1 _Nonnull func, void* _Nullable arg, VoidFunc_0 _Nonnull ret_func);

extern _Noreturn cpu_non_recoverable_error(void);
extern _Noreturn mem_non_recoverable_error(void);

#endif /* _CPU_H */
