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

static ErrorCode GemDosExecutableLoader_RelocExecutable(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pRelocBase, Byte* pTextBase)
{
    const Int32 firstRelocOffset = *((UInt32*)pRelocBase);

    if (firstRelocOffset == 0) {
        return EOK;
    }

    const UInt32 offset = (UInt32)pTextBase;
    Byte* pLoc = (Byte*) (offset + firstRelocOffset);
    UInt8* p = (UInt8*) (pRelocBase + sizeof(UInt32));
    Bool done = false;

    *((UInt32*) pLoc) += offset;

    while (!done) {
        const UInt8 b = *p++;

        switch (b) {
            case 0:
                done = true;
                break;

            case 1:
                pLoc += 254;
                break;

            default:
                pLoc += b;
                *((UInt32*) pLoc) += offset;
                break;
        }
    }
    
    return EOK;
}

ErrorCode GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pExecAddr, Byte* _Nullable * _Nonnull pOutImageBase, Byte* _Nullable * _Nonnull pOutEntryPoint)
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
        return ENOEXEC;
    }
    if (pExecHeader->data_size < 0
        || pExecHeader->bss_size < 0
        || pExecHeader->symbol_table_size < 0) {
        return E2BIG;   // these fields are really unsigned
    }
    if (pExecHeader->is_absolute != 0) {
        return EINVAL;
    }


    // Allocate the text, data and BSS segments 
    const Int nbytes_to_copy = sizeof(GemDosExecutableHeader) + pExecHeader->text_size + pExecHeader->data_size;
    const Int nbytes_to_alloc = __Ceil_PowerOf2(nbytes_to_copy + pExecHeader->bss_size, CPU_PAGE_SIZE);
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
