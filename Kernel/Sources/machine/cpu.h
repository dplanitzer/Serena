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
#include <machine/hw/m68k/cpu.h>
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

// Injects a call to sigurgent() into user space of the currently active vcpu.
// This is done if we detect that the currently running vcpu is running in user
// space and has a signal pending that requires urgent delivery. The sigurgent()
// system call itself does nothing but it gives the system call handler a chance
// to look at the pending signal and handle it as required.
// Note that we set up the sigurgent() injection in such a way that it can return
// back to user space and that the vcpu will be able to continue with whatever
// it was doing before the injection. We do this by pushing a RTS frame on the
// vcpu's user stack that will guide the vcpu back to the original point of
// interruption.
// Also note that we ensure that we do not try to inject a sigurgent while the
// vcpu is still executing inside an earlier sigurgent injection.
extern bool cpu_inject_sigurgent(excpt_frame_t* _Nonnull efp);

extern _Noreturn cpu_non_recoverable_error(uint32_t rgb4);


extern uintptr_t usp_get(void);
extern void usp_set(uintptr_t sp);

#endif /* _CPU_H */
