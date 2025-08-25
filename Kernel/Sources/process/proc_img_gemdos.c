//
//  proc_img_gemdos.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "proc_img_gemdos.h"
#include <filesystem/InodeChannel.h>
#include <kern/string.h>


static void _proc_img_gemdos_reloc(proc_img_t* _Nonnull self, uint8_t* _Nonnull reloc_base, uint8_t* txt_base)
{
    const int32_t firstRelocOffset = *((uint32_t*)reloc_base);

    if (firstRelocOffset == 0) {
        return;
    }

    const uint32_t offset = (uint32_t)txt_base;
    uint8_t* pLoc = (uint8_t*) (offset + firstRelocOffset);
    uint8_t* p = (uint8_t*) (reloc_base + sizeof(uint32_t));
    bool done = false;

    *((uint32_t*) pLoc) += offset;

    while (!done) {
        const uint8_t b = *p++;

        switch (b) {
            case 0:
                done = true;
                break;

            case 1:
                pLoc += 254;
                break;

            default:
                pLoc += b;
                *((uint32_t*) pLoc) += offset;
                break;
        }
    }
}

errno_t _proc_img_load_gemdos_exec(proc_img_t* _Nonnull self, InodeChannelRef _Nonnull chan)
{
    decl_try_err();
    off_t fileOffset;
    struct stat inf;
    gemdos_hdr_t hdr;
    ssize_t nBytesRead;

    try(IOChannel_GetFileInfo(chan, &inf));

    // Do some basic file validation
    if (!S_ISREG(inf.st_mode)) {
        throw(EACCESS);
    }
    if (inf.st_size < sizeof(gemdos_hdr_t)) {
        throw(ENOEXEC);
    }
    else if (inf.st_size > SIZE_MAX) {
        throw(ENOMEM);
    }


    // Read the executable header
    try(IOChannel_Read((IOChannelRef)chan, &hdr, sizeof(hdr), &nBytesRead));

//    printf("magic: %hx\n", hdr.magic);
//    printf("text: %d\n", hdr.text_size);
//    printf("data: %d\n", hdr.data_size);
//    printf("bss: %d\n", hdr.bss_size);
//    printf("symbols: %d\n", hdr.symbol_table_size);
//    printf("file-size: %lld\n", fileSize);
//    while(true);


    // Validate the header (somewhat anyway)
    if (nBytesRead < sizeof(gemdos_hdr_t)) {
        throw(ENOEXEC);
    }

    if (hdr.magic != GEMDOS_EXEC_MAGIC) {
        throw(ENOEXEC);
    }
    if (hdr.text_size <= 0) {
        throw(EINVAL);
    }
    if (hdr.data_size < 0
        || hdr.bss_size < 0
        || hdr.symbol_table_size < 0) {
        throw(EINVAL);   // these fields are really unsigned
    }
    if (hdr.is_absolute != 0) {
        throw(EINVAL);
    }


    // Allocate the text, data and BSS segments 
    const size_t nbytes_to_read = sizeof(gemdos_hdr_t) + hdr.text_size + hdr.data_size;
    const size_t fileOffset_to_reloc = nbytes_to_read + hdr.symbol_table_size;
    const size_t reloc_size = (size_t)(inf.st_size - fileOffset_to_reloc);
    const size_t nbytes_to_alloc = __Ceil_PowerOf2(nbytes_to_read + __max(hdr.bss_size, reloc_size), CPU_PAGE_SIZE);
    uint8_t* img_base = NULL;
    try(AddressSpace_Allocate(&self->as, nbytes_to_alloc, (void**)&img_base));


    // Read the executable header, text and data segments into memory
    IOChannel_Seek((IOChannelRef)chan, 0ll, NULL, SEEK_SET);
    try(IOChannel_Read((IOChannelRef)chan, img_base, nbytes_to_read, &nBytesRead));
    if (nBytesRead != nbytes_to_read) {
        throw(EIO);
    }


    // Read the relocation information into memory
    uint8_t* reloc_base = img_base + nbytes_to_read;
    IOChannel_Seek((IOChannelRef)chan, fileOffset_to_reloc, NULL, SEEK_SET);
    try(IOChannel_Read((IOChannelRef)chan, reloc_base, reloc_size, &nBytesRead));
    if (nBytesRead != reloc_size) {
        throw(EIO);
    }


    // Relocate the executable
    uint8_t* txt_base = img_base + sizeof(gemdos_hdr_t);
    _proc_img_gemdos_reloc(self, reloc_base, txt_base);


    // Initialize the BSS segment
    memset(img_base + nbytes_to_read, 0, hdr.bss_size);


    // Return the result pointers
    self->base = img_base; 
    self->entry_point = txt_base;

catch:
    return err;
}
