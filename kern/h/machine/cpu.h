//
//  machine/cpu.h
//  kpi
//
//  Created by Dietmar Planitzer on 12/24/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _MACHINE_CPU_H
#define _MACHINE_CPU_H 1

#ifdef __M68K__
#include <machine/m68k/cpu.h>
#else
#error "unknown CPU architecture"
#endif


typedef unsigned long   cpu_type_t;
typedef unsigned long   cpu_subtype_t;


#define _CPU_DEF(__isa, __flags) \
(cpu_type_t)(((__flags) & 0xff) << 24) | ((__isa) & 0xffffff)

#define CPU_FLAG_BIG_ENDIAN     0
#define CPU_FLAG_LITTLE_ENDIAN  1

#define CPU_FLAG_32BIT  0
#define CPU_FLAG_64BIT  1

#define CPU_TYPE_68K        _CPU_DEF(0x01, CPU_FLAG_32BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_88K        _CPU_DEF(0x02, CPU_FLAG_32BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_PPC        _CPU_DEF(0x03, CPU_FLAG_32BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_PPC_64     _CPU_DEF(0x03, CPU_FLAG_64BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_x86        _CPU_DEF(0x04, CPU_FLAG_32BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_x86_64     _CPU_DEF(0x04, CPU_FLAG_64BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_i860       _CPU_DEF(0x05, CPU_FLAG_32BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_i960       _CPU_DEF(0x06, CPU_FLAG_32BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_MIPS       _CPU_DEF(0x07, CPU_FLAG_32BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_MIPS_64    _CPU_DEF(0x07, CPU_FLAG_64BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_SPARC      _CPU_DEF(0x08, CPU_FLAG_32BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_SPARC_64   _CPU_DEF(0x08, CPU_FLAG_64BIT|CPU_FLAG_BIG_ENDIAN)
#define CPU_TYPE_ALPHA      _CPU_DEF(0x09, CPU_FLAG_64BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_ARM32      _CPU_DEF(0x0a, CPU_FLAG_32BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_ARM64      _CPU_DEF(0x0a, CPU_FLAG_64BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_RISC_V     _CPU_DEF(0x0b, CPU_FLAG_32BIT|CPU_FLAG_LITTLE_ENDIAN)
#define CPU_TYPE_RISC_V_64  _CPU_DEF(0x0b, CPU_FLAG_64BIT|CPU_FLAG_LITTLE_ENDIAN)


// M68K
#define _CPU_68K_DEF(__family, __fpu) \
(cpu_type_t)((((__fpu) & 0xff) << 8) | ((__family) & 0xff))

#define CPU_FAMILY_68000    1
#define CPU_FAMILY_68010    2
#define CPU_FAMILY_68020    3
#define CPU_FAMILY_68030    4
#define CPU_FAMILY_68040    5
#define CPU_FAMILY_68060    6

#define CPU_FPU_NONE    0
#define CPU_FPU_68881   1
#define CPU_FPU_68882   2
#define CPU_FPU_68040   3
#define CPU_FPU_68060   4

#ifdef KERNEL
#define CPU_MMU_NONE    0
#define CPU_MMU_68451   1
#define CPU_MMU_68851   2
#define CPU_MMU_68030   3
#define CPU_MMU_68040   4
#define CPU_MMU_68060   5
#endif

#define CPU_SUBTYPE_68000       _CPU_68K_DEF(CPU_FAMILY_68000, 0)
#define CPU_SUBTYPE_68010       _CPU_68K_DEF(CPU_FAMILY_68010, 0)
#define CPU_SUBTYPE_68020       _CPU_68K_DEF(CPU_FAMILY_68020, 0)
#define CPU_SUBTYPE_68020_68881 _CPU_68K_DEF(CPU_FAMILY_68020, CPU_FPU_68881)
#define CPU_SUBTYPE_68020_68882 _CPU_68K_DEF(CPU_FAMILY_68020, CPU_FPU_68882)
#define CPU_SUBTYPE_68030       _CPU_68K_DEF(CPU_FAMILY_68030, 0)
#define CPU_SUBTYPE_68030_68881 _CPU_68K_DEF(CPU_FAMILY_68030, CPU_FPU_68881)
#define CPU_SUBTYPE_68030_68882 _CPU_68K_DEF(CPU_FAMILY_68030, CPU_FPU_68882)
#define CPU_SUBTYPE_68040       _CPU_68K_DEF(CPU_FAMILY_68020, CPU_FPU_68040)
#define CPU_SUBTYPE_68LC040     _CPU_68K_DEF(CPU_FAMILY_68020, 0)
#define CPU_SUBTYPE_68060       _CPU_68K_DEF(CPU_FAMILY_68060, CPU_FPU_68060)
#define CPU_SUBTYPE_68LC060     _CPU_68K_DEF(CPU_FAMILY_68060, 0)

#define cpu_68k_family(__subtype)   (int)((__subtype) & 0xff)
#define cpu_68k_fpu(__subtype)      (int)(((__subtype) >> 8) & 0xff)

#endif /* _MACHINE_CPU_H */
