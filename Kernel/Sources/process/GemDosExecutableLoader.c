//
//  GemDosExecutableLoader.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "GemDosExecutableLoader.h"


static errno_t GemDosExecutableLoader_RelocExecutable(GemDosExecutableLoader* _Nonnull self, uint8_t* _Nonnull pRelocBase, uint8_t* pTextBase)
{
    const int32_t firstRelocOffset = *((uint32_t*)pRelocBase);

    if (firstRelocOffset == 0) {
        return EOK;
    }

    const uint32_t offset = (uint32_t)pTextBase;
    uint8_t* pLoc = (uint8_t*) (offset + firstRelocOffset);
    uint8_t* p = (uint8_t*) (pRelocBase + sizeof(uint32_t));
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
    
    return EOK;
}

errno_t GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull self, FilesystemRef _Nonnull pFS, InodeRef _Nonnull pNode, void* _Nullable * _Nonnull pOutImageBase, void* _Nullable * _Nonnull pOutEntryPoint)
{
    decl_try_err();
    FileOffset fileSize = Inode_GetFileSize(pNode);
    IOChannelRef pExecFile = NULL;
    GemDosExecutableHeader hdr;
    ssize_t nBytesRead;

    *pOutImageBase = NULL;
    *pOutEntryPoint = NULL;


    // Do some basic file size validation
    if (fileSize < sizeof(GemDosExecutableHeader)) {
        throw(ENOEXEC);
    }
    else if (fileSize > SIZE_MAX) {
        throw(ENOMEM);
    }


    // Read the executable header
    try(IOResource_Open(pFS, pNode, kOpen_Read, self->user, &pExecFile));
    try(IOResource_Read(pFS, pExecFile, &hdr, sizeof(hdr), &nBytesRead));

//    print("magic: %hx\n", hdr.magic);
//    print("text: %d\n", hdr.text_size);
//    print("data: %d\n", hdr.data_size);
//    print("bss: %d\n", hdr.bss_size);
//    print("symbols: %d\n", hdr.symbol_table_size);
//    print("file-size: %lld\n", fileSize);
//    while(true);


    // Validate the header (somewhat anyway)
    if (nBytesRead < sizeof(GemDosExecutableHeader)) {
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
    const size_t nbytes_to_read = sizeof(GemDosExecutableHeader) + hdr.text_size + hdr.data_size;
    const size_t fileOffset_to_reloc = nbytes_to_read + hdr.symbol_table_size;
    const size_t reloc_size = (size_t)(fileSize - fileOffset_to_reloc);
    const size_t nbytes_to_alloc = __Ceil_PowerOf2(nbytes_to_read + __max(hdr.bss_size, reloc_size), CPU_PAGE_SIZE);
    uint8_t* pImageBase = NULL;
    try(AddressSpace_Allocate(self->addressSpace, nbytes_to_alloc, (void**)&pImageBase));


    // Read the executable header, text and data segments into memory
    File_SetOffset(pExecFile, 0ll);
    try(IOResource_Read(pFS, pExecFile, pImageBase, nbytes_to_read, &nBytesRead));
    if (nBytesRead != nbytes_to_read) {
        throw(EIO);
    }


    // Read the relocation information into memory
    uint8_t* pRelocBase = pImageBase + nbytes_to_read;
    File_SetOffset(pExecFile, fileOffset_to_reloc);
    try(IOResource_Read(pFS, pExecFile, pRelocBase, reloc_size, &nBytesRead));
    if (nBytesRead != reloc_size) {
        throw(EIO);
    }


    // Relocate the executable
    uint8_t* pTextBase = pImageBase + sizeof(GemDosExecutableHeader);
    try(GemDosExecutableLoader_RelocExecutable(self, pRelocBase, pTextBase));


    // Initialize the BSS segment
    memset(pImageBase + nbytes_to_read, 0, hdr.bss_size);


    // Return the result pointers
    *pOutImageBase = pImageBase; 
    *pOutEntryPoint = pTextBase;

catch:
    // XXX should free pImageBase if it exists
    if (pExecFile) {
        IOResource_Close(pFS, pExecFile);
        Object_Release(pExecFile);
    }

    return err;
}
