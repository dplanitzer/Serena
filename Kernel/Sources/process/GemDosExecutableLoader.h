//
//  GemDosExecutableLoader.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef GemDosExecutableLoader_h
#define GemDosExecutableLoader_h

#include <klib/klib.h>
#include "AddressSpace.h"

// <http://toshyp.atari.org/en/005005.html> and Atari GEMDOS Reference Manual
// Why?? 'cause it's easy
#define GEMDOS_EXEC_MAGIC    ((uint16_t) 0x601a)
typedef struct _GemDosExecutableHeader {
    uint16_t    magic;
    int32_t     text_size;
    int32_t     data_size;
    int32_t     bss_size;
    int32_t     symbol_table_size;
    int32_t     reserved;
    uint32_t    flags;
    uint16_t    is_absolute;    // == 0 -> relocatable executable
} GemDosExecutableHeader;


typedef struct _GemDosExecutableLoader {
    AddressSpaceRef _Nonnull    addressSpace;
} GemDosExecutableLoader;


extern void GemDosExecutableLoader_Init(GemDosExecutableLoader* _Nonnull pLoader, AddressSpaceRef _Nonnull pTargetAddressSpace);
extern void GemDosExecutableLoader_Deinit(GemDosExecutableLoader* _Nonnull pLoader);

extern errno_t GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, void* _Nonnull pExecAddr, void* _Nullable * _Nonnull pOutImageBase, void* _Nullable * _Nonnull pOutEntryPoint);

#endif /* GemDosExecutableLoader_h */
