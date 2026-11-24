//
//  utils_win32.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/15/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef UTILS_WIN32_H
#define UTILS_WIN32_H

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>

// The code page of narrow host paths: the one the C runtime uses for argv and
// fopen() (UTF-8 in a UTF-8 locale, otherwise the ANSI or OEM code page)
extern UINT win32_code_page(void);

/* Maps a Win32 error code to the closest errno value. */
extern int win32_errno_from_error(DWORD err);

/* Sets errno from GetLastError(). */
extern void win32_set_errno(void);

/*
 * Converts 'src_len' wide characters (no NUL) to the narrow encoding. With
 * dst == NULL and dst_size == 0 only the required size is computed. Returns
 * the number of bytes written / required (excluding NUL) or -1 with errno
 * set. Characters that can not be represented exactly fail with EILSEQ
 * rather than being replaced with '?' or a best-fit lookalike, since such a
 * path would not refer to the same file.
 */
extern int win32_wide_to_narrow(UINT cp, const wchar_t *src, int src_len, char *dst, int dst_size);

/*
 * Converts a NUL terminated narrow string to a newly allocated wide string
 * (free() it). Returns NULL with errno set on failure (EILSEQ for invalid
 * input).
 */
extern wchar_t *win32_narrow_to_wide(UINT cp, const char *src);

/*
 * Converts a narrow path to a newly allocated, absolute, "\\?\" prefixed
 * wide path ("\\?\UNC\..." for network paths), so MAX_PATH doesn't apply.
 * Returns NULL with errno set on failure.
 */
extern wchar_t *win32_make_long_path(UINT cp, const char *path);
#endif /* _WIN32 */

#endif /* UTILS_WIN32_H */
