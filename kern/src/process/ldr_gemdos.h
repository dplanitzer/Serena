//
//  ldr_gemdos.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _LDR_GEMDOS_H
#define _LDR_GEMDOS_H

#include "proc_img.h"

// <http://toshyp.atari.org/en/005005.html> and Atari GEMDOS Reference Manual
// Why?? 'cause it's easy
#define GEMDOS_EXEC_MAGIC    ((uint16_t) 0x601a)
typedef struct gemdos_hdr {
    uint16_t    magic;
    int32_t     text_size;
    int32_t     data_size;
    int32_t     bss_size;
    int32_t     symbol_table_size;
    int32_t     reserved;
    uint32_t    flags;
    uint16_t    is_absolute;    // == 0 -> relocatable executable
} gemdos_hdr_t;


// Loads a GemDOS executable from the file 'pNode' stored in the filesystem 'pFS'
// into a newly allocated memory area in the address space for which this loader
// was created. Returns the base address of the in-core executable image and the
// entry address of the executable.
extern errno_t ldr_gemdos_load(proc_img_t* _Nonnull pimg, IOChannelRef _Nonnull chan);

#endif /* _LDR_GEMDOS_H */
