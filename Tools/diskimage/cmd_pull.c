//
//  cmd_pull.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "DiskController.h"
#include <errno.h>
#include <stdio.h>

#define BLOCK_SIZE  4096

errno_t cmd_pull(const char* _Nonnull path, const char* _Nonnull dstPath, const char* _Nonnull dmgPath)
{
    decl_try_err();
    DiskControllerRef self;
    IOChannelRef chan = NULL;
    FILE* fp = NULL;
    char* buf = NULL;

    try(DiskController_CreateWithContentsOfPath(dmgPath, &self));
    try_null(buf, malloc(BLOCK_SIZE), ENOMEM);
    try(FileManager_OpenFile(&self->fm, path, kOpen_Read, &chan));
    try_null(fp, fopen(dstPath, "wb"), errno);

    while (true) {
        ssize_t nBytesRead;
        
        err = IOChannel_Read(chan, buf, BLOCK_SIZE, &nBytesRead);
        if (err != EOK || nBytesRead == 0) {
            break;
        }

        fwrite(buf, 1, nBytesRead, fp);
        if (ferror(fp)) {
            err = errno;
            break;
        }
        else if (feof(fp)) {
            err = EIO;
            break;
        }
    }

catch:
    if (fp) {
        fclose(fp);
    }
    IOChannel_Release(chan);
    free(buf);
    DiskController_Destroy(self);
    return err;
}
