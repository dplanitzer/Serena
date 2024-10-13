//
//  RamFSContainer.h
//  diskimage
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_RamFSContainer_h
#define di_RamFSContainer_h

#include <filesystem/FSContainer.h>
#include "DiskImageFormat.h"


open_class(RamFSContainer, FSContainer,
    uint8_t*                    diskImage;
    size_t                      blockSize;
    LogicalBlockCount           blockCount;
    LogicalBlockAddress         highestWrittenToLba;
    enum DiskImageFormatType    format;
);
open_class_funcs(RamFSContainer, FSContainer,
);
class_ref(RamFSContainer);


extern errno_t RamFSContainer_Create(const DiskImageFormat* _Nonnull pFormat, RamFSContainerRef _Nullable * _Nonnull pOutSelf);
extern void RamFSContainer_Destroy(RamFSContainerRef _Nullable self);

// Writes the contents of the disk to the given path as a regular file.
extern errno_t RamFSContainer_WriteToPath(RamFSContainerRef _Nonnull self, const char* pPath);

#endif /* di_RamFSContainer_h */
