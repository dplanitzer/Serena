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

struct vcpu;


extern const char* _Nonnull cpu_get_model_name(int8_t cpu_model);
extern const char* _Nonnull fpu_get_model_name(int8_t fpu_model);

extern int cpu_verify_ram_4b(void* pSrc);

extern int cpu_guarded_read(void* _Nonnull src, void* _Nonnull buffer, int buffer_size);
extern int cpu_guarded_write(void* _Nonnull dst, const void* _Nonnull buffer, int buffer_size);

extern void cpu_sleep(int cpu_type);
extern void cpu_halt(void);

// Called by the HAL when a CPU exception is triggered. 'vp' is the vcpu in
// question. Its 'excpt_sa' field points to a cpu_savearea_t that has the saved
// CPU state and the exception frame set up appropriately.
extern void cpu_exception(struct vcpu* _Nonnull vp, excpt_0_frame_t* _Nonnull utp);
extern void cpu_exception_return(struct vcpu* _Nonnull vp);

// User space function to trigger the return from an exception handler
extern void excpt_return(void);

extern _Noreturn cpu_non_recoverable_error(uint32_t rgb4);


extern uintptr_t usp_get(void);
extern void usp_set(uintptr_t sp);

extern uintptr_t sp_push_ptr(uintptr_t sp, void* _Nonnull ptr);
extern uintptr_t sp_push_bytes(uintptr_t sp, const void* _Nonnull p, size_t nbytes);
#define sp_push_rts(__sp, __pc) sp_push_ptr(__sp, __pc)

#endif /* _CPU_H */
