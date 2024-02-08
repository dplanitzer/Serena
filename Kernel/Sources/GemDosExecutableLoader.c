//
//  GemDosExecutableLoader.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "GemDosExecutableLoader.h"


void GemDosExecutableLoader_Init(GemDosExecutableLoader* _Nonnull pLoader, AddressSpaceRef _Nonnull pTargetAddressSpace)
{
    pLoader->addressSpace = pTargetAddressSpace;
}

void GemDosExecutableLoader_Deinit(GemDosExecutableLoader* _Nonnull pLoader)
{
    pLoader->addressSpace = NULL;
}

static errno_t GemDosExecutableLoader_RelocExecutable(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pRelocBase, Byte* pTextBase)
{
    const int32_t firstRelocOffset = *((uint32_t*)pRelocBase);

    if (firstRelocOffset == 0) {
        return EOK;
    }

    const uint32_t offset = (uint32_t)pTextBase;
    Byte* pLoc = (Byte*) (offset + firstRelocOffset);
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

errno_t GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pExecAddr, Byte* _Nullable * _Nonnull pOutImageBase, Byte* _Nullable * _Nonnull pOutEntryPoint)
{
    decl_try_err();
    GemDosExecutableHeader* pExecHeader = (GemDosExecutableHeader*)pExecAddr;

    //print("text: %d\n", pExecHeader->text_size);
    //print("data: %d\n", pExecHeader->data_size);
    //print("bss: %d\n", pExecHeader->bss_size);
    //while(true);


    // Validate the header (somewhat anyway)
    if (pExecHeader->magic != GEMDOS_EXEC_MAGIC) {
        return ENOEXEC;
    }
    if (pExecHeader->text_size <= 0) {
        return EINVAL;
    }
    if (pExecHeader->data_size < 0
        || pExecHeader->bss_size < 0
        || pExecHeader->symbol_table_size < 0) {
        return EINVAL;   // these fields are really unsigned
    }
    if (pExecHeader->is_absolute != 0) {
        return EINVAL;
    }


    // Allocate the text, data and BSS segments 
    const int nbytes_to_copy = sizeof(GemDosExecutableHeader) + pExecHeader->text_size + pExecHeader->data_size;
    const int nbytes_to_alloc = __Ceil_PowerOf2(nbytes_to_copy + pExecHeader->bss_size, CPU_PAGE_SIZE);
    Byte* pImageBase = NULL;
    try(AddressSpace_Allocate(pLoader->addressSpace, nbytes_to_alloc, &pImageBase));


    // Copy the executable header, text and data segments
    Bytes_CopyRange(pImageBase, pExecAddr, nbytes_to_copy);


    // Initialize the BSS segment
    Bytes_ClearRange(pImageBase + nbytes_to_copy, pExecHeader->bss_size);

    // Relocate the executable
    Byte* pRelocBase = pExecAddr
        + sizeof(GemDosExecutableHeader)
        + pExecHeader->text_size
        + pExecHeader->data_size
        + pExecHeader->symbol_table_size;
    Byte* pTextBase = pImageBase
        + sizeof(GemDosExecutableHeader);

    try(GemDosExecutableLoader_RelocExecutable(pLoader, pRelocBase, pTextBase));

    // Return the result pointers
    *pOutImageBase = pImageBase; 
    *pOutEntryPoint = pTextBase;

    return EOK;

catch:
    // XXX should free pImageBase if it exists
    *pOutImageBase = NULL;
    *pOutEntryPoint = NULL;
    return err;
}
