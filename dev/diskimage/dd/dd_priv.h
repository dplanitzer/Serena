//
//  dd_priv.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_PRIV_H
#define DD_PRIV_H

#include "RamContainer.h"
#include <filemanager/FileManager.h>
#include <filesystem/Filesystem.h>
#include <stdbool.h>


struct dd_fs {
    RamContainerRef _Nonnull    fsContainer;
    FilesystemRef _Nonnull      fs;
    FileManager                 fm;
    bool                        isFsUp;
    bool                        isFmUp;
};

#endif /* DD_PRIV_H */
