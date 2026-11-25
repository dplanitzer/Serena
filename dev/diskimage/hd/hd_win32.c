//
//  hd_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifdef _WIN32

#include "hd.h"
#include "utils_win32.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int hd_path_kind(const char *path)
{
    wchar_t *wpath = win32_narrow_to_wide(win32_code_page(), path);
    if (wpath == NULL) {
        return HD_KIND_ERROR;
    }

    /* "file\" would be an invalid name; a root keeps its separator ("C:\", "\") */
    size_t len = wcslen(wpath);
    while (len > 1 && (wpath[len - 1] == L'\\' || wpath[len - 1] == L'/')
           && wpath[len - 2] != L':') {
        wpath[--len] = L'\0';
    }

    const DWORD attr = GetFileAttributesW(wpath);
    const DWORD err = GetLastError();
    free(wpath);

    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return HD_KIND_NONE;
        }
        errno = win32_errno_from_error(err);
        return HD_KIND_ERROR;
    }
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? HD_KIND_DIRECTORY : HD_KIND_OTHER;
}

int hd_create_dir(const char *path, int *created)
{
    UINT cp = win32_code_page();
    wchar_t* wpath = win32_make_long_path(cp, path);
    if (wpath == NULL) {
        return -1;
    }


    BOOL ok = CreateDirectoryW(wpath, NULL);
    free(wpath);

    if (ok) {
        if (created) {
            *created = 1;
        }
    } else {
        DWORD err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            errno = win32_errno_from_error(err);
            return -1;
        }
        DWORD attr = GetFileAttributesW(wpath);
        if (attr == INVALID_FILE_ATTRIBUTES) {
            errno = win32_errno_from_error(GetLastError());
            return -1;
        }
        if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            errno = ENOTDIR;
            return -1;
        }
        if (created) {
            *created = 0;
        }
    }

    return 0;
}

FILE *hd_fopen(const char *path, const char *mode)
{
    UINT cp = win32_code_page();
    wchar_t* wpath = win32_make_long_path(cp, path);
    if (wpath == NULL) {
        return NULL;
    }

    wchar_t wmode[8];
    size_t i = 0;
    for (; mode[i] != '\0' && i < sizeof(wmode); i++) {
        wmode[i] = (wchar_t)(unsigned char)mode[i];
    }
    wmode[i] = L'\0';

    FILE *fp = _wfopen(wpath, wmode);
    free(wpath);

    return fp;
}

#endif  /* _WIN32 */
