//
//  FSManager.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/7/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef FSManager_h
#define FSManager_h

#include "RamContainer.h"
#include <filemanager/FileManager.h>
#include <filesystem/Filesystem.h>
#include <System/File.h>
#include <stdbool.h>


typedef struct FSManager {
    FilesystemRef _Nonnull  fs;
    FileManager             fm;
    bool                    isFmUp;
} FSManager;
typedef FSManager* FSManagerRef;


extern errno_t FSManager_Create(RamContainerRef _Nonnull fsContainer, FSManagerRef _Nullable * _Nonnull pOutSelf);
extern void FSManager_Destroy(FSManagerRef _Nullable self);

#endif /* FSManager_h */
