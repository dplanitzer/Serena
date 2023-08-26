//
//  GemDosExecutableLoader.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "GemDosExecutableLoader.h"
#include "Bytes.h"


void GemDosExecutableLoader_Init(GemDosExecutableLoader* _Nonnull pLoader, AddressSpaceRef _Nonnull pTargetAddressSpace)
{
    pLoader->addressSpace = pTargetAddressSpace;
}

void GemDosExecutableLoader_Deinit(GemDosExecutableLoader* _Nonnull pLoader)
{
    pLoader->addressSpace = NULL;
}

static ErrorCode GemDosExecutableLoader_RelocExecutable(GemDosExecutableLoader* _Nonnull pLoader, Int32* _Nonnull pRelocPtr, Int32 offset)
{    
    if (*pRelocPtr == 0) {
        return EOK;
    }

    Byte* pLocPtr = (Byte*) (offset + *pRelocPtr);
    //print("reloc: 0x%p\n", pLocPtr);

    *((Int32*) pLocPtr) += offset;
    UInt8* p = (UInt8*) (pRelocPtr + 1);
    Bool done = false;

    while (!done) {
        const UInt8 b = *p;

        //if (b != 0) print("reloc: b: %d, l: 0x%p\n", (Int32)b, pLocPtr);
        switch (b) {
            case 0:
                done = true;
                break;

            case 1:
                pLocPtr += 254;
                break;

            default:
                pLocPtr += b;
                *((Int32*) pLocPtr) += offset;
                break;
        }
        p++;
    }
    
    return EOK;
}

ErrorCode GemDosExecutableLoader_Load(GemDosExecutableLoader* _Nonnull pLoader, Byte* _Nonnull pExecAddr, Byte* _Nonnull * _Nonnull pEntryPoint)
{
    decl_try_err();
    GemDosExecutableHeader* pExecHeader = (GemDosExecutableHeader*)pExecAddr;

    //print("text: %d\n", pExecHeader->text_size);
    //print("data: %d\n", pExecHeader->data_size);
    //print("bss: %d\n", pExecHeader->bss_size);
    //while(true);
    // XXX for now to keep loading simpler
    assert(AddressSpace_IsEmpty(pLoader->addressSpace));


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
        return EINVAL;
    }
    if (pExecHeader->is_absolute != 0) {
        return EINVAL;
    }


    // Allocate the text, data and BSS segments 
    const Int32 nbytes_to_copy = pExecHeader->text_size + pExecHeader->data_size;
    const Int32 nbytes_to_alloc = nbytes_to_copy + pExecHeader->bss_size;
    Byte* pBasePtr = NULL;
    
    try(AddressSpace_Allocate(pLoader->addressSpace, nbytes_to_alloc, &pBasePtr));


    // Copy the text and data segments
    Bytes_CopyRange(pBasePtr, pExecAddr + sizeof(GemDosExecutableHeader), nbytes_to_copy);


    // Initialize the BSS segment
    Bytes_ClearRange(pBasePtr + nbytes_to_copy, pExecHeader->bss_size);


    // Relocate the executable
    Int32* pRelocPtr = (Int32*)(pExecAddr
        + sizeof(GemDosExecutableHeader)
        + pExecHeader->text_size
        + pExecHeader->data_size
        + pExecHeader->bss_size
        + pExecHeader->symbol_table_size);

    try(GemDosExecutableLoader_RelocExecutable(pLoader, pRelocPtr, (Int32)pBasePtr));


    // Calculate the entry point
    *pEntryPoint = pBasePtr;

    return EOK;

catch:
    return err;
}
