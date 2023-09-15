//
//  GemDosExecutableLoader.h
//  Apollo
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
#define GEMDOS_EXEC_MAGIC    ((UInt16) 0x601a)
typedef struct _GemDosExecutableHeader {
    UInt16  magic;
    Int32   text_size;
    Int32   data_size;
    Int32   bss_size;
    Int32   symbol_table_size;
    Int32   reserved;
    UInt32  flags;
    UInt16  is_absolute;    // == 0 -> relocatable executable
} GemDosExecutableHeader;


typedef struct _GemDosExecutableLoader {
    AddressSpaceRef _Nonnull    addressSpace;
} GemDosExecutableLoader;


extern void GemDosExecutableLoader_Init(GemDosExecutableLoader* _Nonnull pLoader, AddressSpaceRef _Nonnull pTargetAddressSpace);
extern void GemDosExecutableLoader_Deinit(GemDosExecutableLoader* _Nonnull pLoader);

extern ErrorCode GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pExecAddr, Byte* _Nullable * _Nonnull pOutImageBase, Byte* _Nullable * _Nonnull pOutEntryPoint);

#endif /* GemDosExecutableLoader_h */
