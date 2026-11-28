//
//  RamContainer.h
//  diskimage
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef RamContainer_h
#define RamContainer_h

#include <filesystem/FSContainer.h>
#include <stdbool.h>
#include <stdio.h>
#include "DiskImageFormat.h"


open_class(RamContainer, FSContainer,
    FILE* _Nonnull              fp;
    uint8_t* _Nonnull           diskImage;
    uint8_t* _Nonnull           mappedFlags;    // One flag per block: false -> block not mapped; true -> block is mapped
    size_t                      blockShift;
    size_t                      blockMask;
    blkno_t                     lowestWrittenToLba;
    blkno_t                     highestWrittenToLba;
    enum DiskImageFormatType    format;
    bool                        isDirty;
);
open_class_funcs(RamContainer, FSContainer,
);
class_ref(RamContainer);


enum {
    kRamContainer_OverwriteExisting = 1     // RamContainer_Create()
};


extern errno_t RamContainer_Create(const char* _Nonnull path, const DiskImageFormat* _Nonnull format, int flags, RamContainerRef _Nullable * _Nonnull pOutSelf);
extern errno_t RamContainer_Open(const char* _Nonnull path, RamContainerRef _Nullable * _Nonnull pOutSelf);

extern void RamContainer_Destroy(RamContainerRef _Nullable self);

extern errno_t RamContainer_Read(RamContainerRef _Nonnull self, void* _Nonnull buf, ssize_t nBytesToRead, off_t offset, ssize_t* _Nonnull pOutBytesRead);
extern errno_t RamContainer_Write(RamContainerRef _Nonnull self, const void* _Nonnull buf, ssize_t nBytesToWrite, off_t offset, ssize_t* _Nonnull pOutBytesWritten);

// Overrides all disk data with 0
extern void RamContainer_WipeDisk(RamContainerRef _Nonnull self);

// Syncs the in-memory disk image to its backing file on disk.
extern errno_t RamContainer_Sync(RamContainerRef _Nonnull self);

#endif /* RamContainer_h */
