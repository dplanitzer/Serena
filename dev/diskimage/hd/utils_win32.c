//
//  utils_win32.c
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifdef _WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601     /* Windows 7: WC_ERR_INVALID_CHARS */
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "utils_win32.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

// Whether the C runtime's current locale is UTF-8, e.g. after
// setlocale(LC_ALL, ".UTF-8")
static bool win32_locale_is_utf8(void)
{
    const char* loc = setlocale(LC_CTYPE, NULL);
    const char* dot = (loc != NULL) ? strrchr(loc, '.') : NULL;

    if (dot == NULL) {
        return false;
    }
    dot++;
    return _stricmp(dot, "utf8") == 0 || _stricmp(dot, "utf-8") == 0;
}

UINT win32_code_page(void)
{
    // The same rule the C runtime uses for argv and for the file names it
    // passes to Windows (fopen() etc.), so that paths from the command line,
    // from the CRT and from these functions all agree. Without a UTF-8
    // locale this is the ANSI code page, which is UTF-8 only if the
    // executable's manifest or the system-wide "Beta: Use Unicode UTF-8"
    // option asks for it.
    if (win32_locale_is_utf8()) {
        return CP_UTF8;
    }
    return AreFileApisANSI() ? GetACP() : GetOEMCP();
}

int win32_errno_from_error(DWORD err)
{
    switch (err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return EACCES;
    case ERROR_PRIVILEGE_NOT_HELD:
        return EPERM;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_DIRECTORY:
        return ENOTDIR;
    case ERROR_DIR_NOT_EMPTY:
        return ENOTEMPTY;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return ENOSPC;
    case ERROR_WRITE_PROTECT:
        return EROFS;
    case ERROR_TOO_MANY_OPEN_FILES:
        return EMFILE;
    case ERROR_FILENAME_EXCED_RANGE:
        return ENAMETOOLONG;
    case ERROR_NOT_SUPPORTED:
    case ERROR_INVALID_FUNCTION:
        return ENOTSUP;
    case ERROR_INVALID_NAME:
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    case ERROR_NO_UNICODE_TRANSLATION:
        return EILSEQ;
    default:
        return EIO;
    }
}

void win32_set_errno(void)
{
    errno = win32_errno_from_error(GetLastError());
}

wchar_t *win32_narrow_to_wide(UINT cp, const char *src)
{
    DWORD flags = MB_ERR_INVALID_CHARS;
    int n = MultiByteToWideChar(cp, flags, src, -1, NULL, 0);

    if (n == 0 && GetLastError() == ERROR_INVALID_FLAGS) {
        /* Some code pages (e.g. ISO-2022, UTF-7) don't support the flag. */
        flags = 0;
        n = MultiByteToWideChar(cp, flags, src, -1, NULL, 0);
    }
    if (n == 0) {
        win32_set_errno();
        return NULL;
    }

    wchar_t *w = malloc((size_t)n * sizeof(wchar_t));
    if (w == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    if (MultiByteToWideChar(cp, flags, src, -1, w, n) == 0) {
        win32_set_errno();
        free(w);
        return NULL;
    }
    return w;
}

int win32_wide_to_narrow(UINT cp, const wchar_t *src, int src_len, char *dst, int dst_size)
{
    BOOL used_default = FALSE;
    int n;

    if (cp == CP_UTF8) {
        n = WideCharToMultiByte(cp, WC_ERR_INVALID_CHARS, src, src_len, dst, dst_size, NULL, NULL);
    } else {
        n = WideCharToMultiByte(cp, WC_NO_BEST_FIT_CHARS, src, src_len, dst, dst_size, NULL, &used_default);
        
        if (n == 0) {
            DWORD err = GetLastError();
            if (err == ERROR_INVALID_FLAGS || err == ERROR_INVALID_PARAMETER) {
                /* Some code pages (e.g. ISO-2022, UTF-7) support neither option. */
                used_default = FALSE;
                n = WideCharToMultiByte(cp, 0, src, src_len, dst, dst_size, NULL, NULL);
            }
        }
    }

    if (n == 0) {
        win32_set_errno();
        return -1;
    }
    if (used_default) {
        errno = EILSEQ;
        return -1;
    }

    return n;
}

wchar_t *win32_make_long_path(UINT cp, const char *path)
{
    wchar_t *wpath = win32_narrow_to_wide(cp, path);
    if (wpath == NULL) {
        return NULL;
    }

    DWORD n = GetFullPathNameW(wpath, 0, NULL, NULL);
    if (n == 0) {
        win32_set_errno();
        free(wpath);
        return NULL;
    }

    wchar_t *full = malloc((size_t)n * sizeof(wchar_t));
    if (full == NULL) {
        errno = ENOMEM;
        free(wpath);
        return NULL;
    }

    DWORD m = GetFullPathNameW(wpath, n, full, NULL);
    if (m == 0 || m >= n) {
        /* m >= n: the current directory changed between the two calls */
        if (m == 0) {
            win32_set_errno();
        } else {
            errno = EAGAIN;
        }
        free(full);
        free(wpath);
        return NULL;
    }
    free(wpath);

    const wchar_t *prefix;
    const wchar_t *rest;
    if (wcsncmp(full, L"\\\\?\\", 4) == 0 || wcsncmp(full, L"\\\\.\\", 4) == 0) {
        return full;                    /* already a device / long path */
    } else if (full[0] == L'\\' && full[1] == L'\\') {
        prefix = L"\\\\?\\UNC\\";       /* \\server\share -> \\?\UNC\server\share */
        rest = full + 2;
    } else {
        prefix = L"\\\\?\\";            /* C:\dir -> \\?\C:\dir */
        rest = full;
    }

    size_t prefix_len = wcslen(prefix);
    size_t rest_len = wcslen(rest);
    wchar_t *out = malloc((prefix_len + rest_len + 1) * sizeof(wchar_t));
    if (out == NULL) {
        errno = ENOMEM;
        free(full);
        return NULL;
    }
    memcpy(out, prefix, prefix_len * sizeof(wchar_t));
    memcpy(out + prefix_len, rest, (rest_len + 1) * sizeof(wchar_t));
    free(full);

    return out;
}

#endif  /* _WIN32 */
