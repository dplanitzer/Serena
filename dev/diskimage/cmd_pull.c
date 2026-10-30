//
//  cmd_pull.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include "FSManager.h"
#include <ext/errno.h>
#include <kpi/file.h>
#include <handler/Handler.h>

errno_t cmd_pull(const char* _Nonnull srcPath, const char* _Nonnull dstPath, const char* _Nonnull dmgPath)
{
    decl_try_err();
    RamContainerRef disk = NULL;
    FSManagerRef m = NULL;
    HandlerRef hnd = NULL;
    FILE* fp = NULL;

    try(RamContainer_CreateWithContentsOfPath(dmgPath, &disk));
    try(FSManager_Create(disk, &m));

    try(FileManager_OpenFile(&m->fm, srcPath, O_RDONLY, &hnd));
    try(set_host_path(dstPath));

    if (is_host_directory(g_host_path)) {
        if (last_path_component(DMG_PATH, g_last_path_comp, sizeof(g_last_path_comp), srcPath) < 0) {
            throw(errno);
        }
        if (path_append(HOST_PATH, g_host_path, sizeof(g_host_path), g_last_path_comp) < 0) {
            throw(errno);
        }
    }

    try_null(fp, fopen(g_host_path, "wb"), errno);

    while (true) {
        ssize_t nBytesRead;
        
        err = Handler_Read(hnd, g_copy_buf, sizeof(g_copy_buf), &nBytesRead);
        if (err != EOK || nBytesRead == 0) {
            break;
        }

        fwrite(g_copy_buf, 1, nBytesRead, fp);
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
    Object_Release(hnd);

    FSManager_Destroy(m);
    Object_Release(disk);
    return err;
}
