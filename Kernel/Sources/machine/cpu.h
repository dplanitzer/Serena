//
//  cpu.h
//  Kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _CPU_H
#define _CPU_H 1

#include <kpi/exception.h>
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

// Called by the HAL when a CPU exception is triggered. 'efp' is the CPU
// exception frame on the kernel stack and 'sfp' is an optional and platform
// specific secondary exception frame. Eg 'sfp' is the fsave state frame of a
// 68881/68882 co-processor if it triggered an exception.
extern excpt_func_t _Nonnull cpu_exception(excpt_frame_t* _Nonnull efp, void* _Nullable sfp);
extern void cpu_exception_return(void);

// User space function to trigger the return from an exception handler
extern void excpt_return(void);

extern _Noreturn cpu_non_recoverable_error(void);
extern _Noreturn mem_non_recoverable_error(void);


extern uintptr_t usp_get(void);
extern void usp_set(uintptr_t sp);

extern uintptr_t sp_push_ptr(uintptr_t sp, void* _Nonnull ptr);
extern uintptr_t sp_push_bytes(uintptr_t sp, const void* _Nonnull p, size_t nbytes);
#define sp_push_rts(__sp, __pc) sp_push_ptr(__sp, __pc)

#endif /* _CPU_H */
