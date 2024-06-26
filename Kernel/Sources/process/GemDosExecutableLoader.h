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
#include <filesystem/Inode.h>
#include <User.h>
#include "AddressSpace.h"

// <http://toshyp.atari.org/en/005005.html> and Atari GEMDOS Reference Manual
// Why?? 'cause it's easy
#define GEMDOS_EXEC_MAGIC    ((uint16_t) 0x601a)
typedef struct GemDosExecutableHeader {
    uint16_t    magic;
    int32_t     text_size;
    int32_t     data_size;
    int32_t     bss_size;
    int32_t     symbol_table_size;
    int32_t     reserved;
    uint32_t    flags;
    uint16_t    is_absolute;    // == 0 -> relocatable executable
} GemDosExecutableHeader;


typedef struct GemDosExecutableLoader {
    AddressSpaceRef _Nonnull    addressSpace;
    User                        user;
} GemDosExecutableLoader;


#define GemDosExecutableLoader_Init(__self, __pTargetAddressSpace, __user) \
(__self)->addressSpace = __pTargetAddressSpace; \
(__self)->user = __user

#define GemDosExecutableLoader_Deinit(__self) \
(__self)->addressSpace = NULL


// Loads a GemDOS executable from the file 'pNode' stored in the filesystem 'pFS'
// into a newly allocated memory area in the address space for which this loader
// was created. Returns the base address of the in-core executable image and the
// entry address of the executable.
extern errno_t GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, InodeRef _Nonnull pNode, void* _Nullable * _Nonnull pOutImageBase, void* _Nullable * _Nonnull pOutEntryPoint);

#endif /* GemDosExecutableLoader_h */
