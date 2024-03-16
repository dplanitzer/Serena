//
//  diskimage_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 3/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "diskimage.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VALID_FILE_ATTRIBS (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_DEVICE)


errno_t di_concat_path(const char* _Nonnull basePath, const char* _Nonnull fileName, char* _Nonnull buffer, size_t nBufferSize)
{
    const size_t basePathLen = strlen(basePath);
    const size_t fileNameLen = strlen(fileName);

    if (basePathLen + 1 + fileNameLen > (nBufferSize - 1)) {
        return EINVAL;
    }

    strcpy(buffer, basePath);
    if (fileNameLen > 0) {
        strcat(buffer, "\\");
        strcat(buffer, fileName);
    }

    return EOK;
}

static errno_t _di_recursive_iterate_directory(const char* _Nonnull pBasePath, const char* _Nonnull pDirName, const di_iterate_directory_callbacks* _Nonnull cb, void* _Nullable parentToken)
{
    di_direntry entry;
    errno_t err = EOK;
    char buf[MAX_PATH];

    if (di_concat_path(pBasePath, pDirName, buf, MAX_PATH - 2) != EOK) {
        return EINVAL;
    }
    strcat(buf, "\\*");

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(buf, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return EIO;
    }
    buf[strlen(buf) - 2] = '\0';
   
    do {
        if ((ffd.dwFileAttributes & VALID_FILE_ATTRIBS) == 0) {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0) {
                    void* token = NULL;

                    entry.name = ffd.cFileName;
                    entry.fileSize = 0;
                    entry.permissions = FilePermissions_Make(kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute, kFilePermission_Read | kFilePermission_Write | kFilePermission_Execute, kFilePermission_Read | kFilePermission_Execute);

                    err = cb->beginDirectory(cb->context, buf, &entry, parentToken, &token);
                    if (err == EOK) {
                        err = _di_recursive_iterate_directory(buf, ffd.cFileName, cb, token);
                    }
                    const errno_t err1 = cb->endDirectory(cb->context, token);
                    if (err == EOK && err1 != EOK) {
                        err = err1;
                    }
                }
            }
            else {
                LARGE_INTEGER filesize;

                filesize.LowPart = ffd.nFileSizeLow;
                filesize.HighPart = ffd.nFileSizeHigh;
                entry.name = ffd.cFileName;
                entry.fileSize = filesize.QuadPart;
                entry.permissions = FilePermissions_Make(kFilePermission_Read | kFilePermission_Write, kFilePermission_Read | kFilePermission_Write, kFilePermission_Read | kFilePermission_Write);
                
                const char* extPtr = strrchr(entry.name, '.');
                if (extPtr && !strcmp(extPtr, ".exe")) {
                    FilePermissions_Set(entry.permissions, kFilePermissionsScope_User, kFilePermission_Execute);
                    FilePermissions_Set(entry.permissions, kFilePermissionsScope_Group, kFilePermission_Execute);
                }

                err = cb->file(cb->context, buf, &entry, parentToken);
            }
        }
    } while (err == EOK && FindNextFile(hFind, &ffd) != 0);
    
    if (err == EOK) {
        if (GetLastError() != ERROR_NO_MORE_FILES) {
            err = EIO;
        }
    }

    FindClose(hFind);
    return err;
}

errno_t di_iterate_directory(const char* _Nonnull rootPath, const di_iterate_directory_callbacks* _Nonnull cb, void* _Nullable pInitialToken)
{
    return _di_recursive_iterate_directory(rootPath, "", cb, pInitialToken);
}
