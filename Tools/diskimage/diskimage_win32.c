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

static errno_t _di_recursive_iterate_directory(const char* _Nonnull pBasePath, const char* _Nonnull pDirName, const di_iterate_directory_callbacks* _Nonnull cb, void* _Nullable parentToken)
{
    di_direntry entry;
    size_t nDirName = strlen(pDirName);
    errno_t err = EOK;
    char buf[MAX_PATH];
   
    if (strlen(pBasePath) + 1 + nDirName > (MAX_PATH - 3)) {
        return EINVAL;
    }

    strcpy(buf, pBasePath);
    if (nDirName > 0) {
        strcat(buf, "\\");
        strcat(buf, pDirName);
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

                    err = cb->beginDirectory(cb->context, &entry, parentToken, &token);
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

                err = cb->file(cb->context, &entry, parentToken);
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
