//
//  utils.c
//  diskimage
//
//  Created by Dietmar Planitzer on 12/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifdef _WIN32

#include "path_utils.h"
#include <stdlib.h>
#include <string.h>
#include <locale.h>

// Pure Windows host path functions. Unlike the di_path_type based functions
// above, these understand drive letters ("C:"), UNC shares ("\\server\share")
// and device paths ("\\?\C:\", "\\.\COM1"). Both '\' and '/' are accepted as
// separators, and '\' is generated.
//
// Host paths are in the code page that win32_code_page() returns. In a DBCS
// code page (e.g. Shift-JIS) a '\' byte that is the second half of a
// double-byte character is not a separator.

static bool win32_is_sep(char ch)
{
    return ch == '\\' || ch == '/';
}

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

// Returns the length in bytes of the character at 'p'. UTF-8 multi-byte
// sequences never contain ASCII bytes, so UTF-8 can be scanned byte by byte.
static size_t win32_char_len(UINT cp, const char* _Nonnull p)
{
    return (cp != CP_UTF8 && p[1] != '\0' && IsDBCSLeadByteEx(cp, (BYTE)p[0])) ? 2 : 1;
}

// Returns the index of the first separator or '\0' at or after index 'i'
static size_t win32_skip_name(UINT cp, const char* _Nonnull path, size_t i)
{
    while (path[i] != '\0' && !win32_is_sep(path[i])) {
        i += win32_char_len(cp, &path[i]);
    }
    return i;
}

static bool win32_is_drive(const char* _Nonnull p)
{
    return ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':';
}

static bool win32_ends_with_sep(const char* _Nonnull s, size_t len)
{
    const UINT cp = win32_code_page();
    size_t i = 0, last = 0;

    // Forward, so that the start of the last character is known
    while (i < len) {
        last = i;
        i += win32_char_len(cp, &s[i]);
    }
    return len > 0 && win32_is_sep(s[last]);
}

// Returns the length of the path's root: the prefix that isn't a file or
// directory name, including one root separator if there is one:
//   "C:", "C:\"                        drive (relative / absolute)
//   "\"                                root of the current drive
//   "\\server\share\"                  UNC share
//   "\\?\C:\", "\\.\C:\"               device path to a drive
//   "\\?\UNC\server\share\"            device path to a UNC share
//   "\\?\Volume{...}\", "\\.\COM1"     other device paths
static size_t win32_root_len(UINT cp, const char* _Nonnull path)
{
    size_t i = 0;

    if (win32_is_sep(path[0]) && win32_is_sep(path[1])) {
        if ((path[2] == '?' || path[2] == '.') && win32_is_sep(path[3])) {
            i = 4;
            if (_strnicmp(&path[4], "UNC", 3) == 0 && win32_is_sep(path[7])) {
                i = win32_skip_name(cp, path, 8);            // server
                if (win32_is_sep(path[i])) {
                    i = win32_skip_name(cp, path, i + 1);    // share
                }
            }
            else if (win32_is_drive(&path[4])) {
                i += 2;
            }
            else {
                i = win32_skip_name(cp, path, i);            // device name
            }
        }
        else {
            i = win32_skip_name(cp, path, 2);                // server
            if (win32_is_sep(path[i])) {
                i = win32_skip_name(cp, path, i + 1);        // share
            }
        }
    }
    else if (win32_is_drive(path)) {
        i = 2;
    }

    if (win32_is_sep(path[i])) {
        i++;
    }
    return i;
}

bool win32_is_directory(const char* _Nonnull path)
{
    const UINT cp = win32_code_page();
    const int wide_chars_needed = MultiByteToWideChar(cp, 0, path, -1, NULL, 0);

    if (wide_chars_needed <= 0) {
        return false;
    }

    wchar_t* wide_path = malloc(wide_chars_needed * sizeof(wchar_t));
    if (wide_path == NULL) {
        return false;
    }

    if (MultiByteToWideChar(cp, 0, path, -1, wide_path, wide_chars_needed) == 0) {
        free(wide_path);
        return false;
    }

    const DWORD attributes = GetFileAttributesW(wide_path);

    free(wide_path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

ssize_t win32_path_append(char* _Nullable buf, size_t buf_size, const char* _Nonnull component)
{
    size_t base_len = 0;
    size_t needs_sep = 0;

    if (buf) {
        base_len = strlen(buf);
        // No separator after a bare drive: "C:" + "a" is "C:a", which is
        // relative to the drive's current directory, while "C:\a" is not
        if (base_len > 0 && !win32_ends_with_sep(buf, base_len) && !(base_len == 2 && win32_is_drive(buf))) {
            needs_sep = 1;
        }
    }

    // Leading separators are only dropped when appending to a path. Appended
    // to an empty path, a component stays as it is, so "\Users" stays rooted.
    if (base_len > 0) {
        while (win32_is_sep(*component)) {
            component++;
        }
    }

    const size_t comp_len = strlen(component);

    if (comp_len == 0) {
        needs_sep = 0;
    }

    const size_t total_size = base_len + needs_sep + comp_len + 1;

    if (buf == NULL) {
        return total_size;
    }

    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    if (needs_sep) {
        buf[base_len] = '\\';
        base_len++;
    }

    if (comp_len > 0) {
        memcpy(&buf[base_len], component, comp_len);
    }
    buf[base_len + comp_len] = '\0';

    return 0;
}

ssize_t win32_last_path_component(char* _Nullable buf, size_t buf_size, const char* _Nonnull path)
{
    const UINT cp = win32_code_page();
    const size_t root_len = win32_root_len(cp, path);
    size_t name_start = 0;
    size_t name_len = 0;
    const char* src;
    size_t src_len;

    if (buf && buf_size < 1) {
        errno = EINVAL;
        return -1;
    }


    // Find the last name after the root, ignoring trailing separators
    for (size_t i = root_len; path[i] != '\0';) {
        if (win32_is_sep(path[i])) {
            i++;
        }
        else {
            const size_t end = win32_skip_name(cp, path, i);
            name_start = i;
            name_len = end - i;
            i = end;
        }
    }

    if (name_len > 0) {
        // "C:\a\b\" -> "b", "C:a" -> "a"
        src = &path[name_start];
        src_len = name_len;
    }
    else if (root_len > 0) {
        // Only a root: "C:", "C:\", "\", "\\server\share\"
        src = path;
        src_len = root_len;
    }
    else {
        // Empty path
        src = ".";
        src_len = 1;
    }

    const size_t total_size = src_len + 1;

    if (buf == NULL) {
        return (ssize_t)total_size;
    }


    if (total_size > buf_size) {
        errno = ENAMETOOLONG;
        buf[0] = '\0';
        return -1;
    }

    memmove(buf, src, src_len);
    buf[src_len] = '\0';

    return 0;
}

int win32_path_next(di_path_iter_t* _Nonnull it, const char* _Nonnull path)
{
    const UINT cp = win32_code_page();

    // First-time initialization
    if (it->_next_ptr == NULL) {
        const size_t root_len = win32_root_len(cp, path);

        it->index = -1;
        it->_next_ptr = path;

        // The root is a prefix of the first component, so that appending the
        // components to each other reproduces the path:
        // "C:\Users\me" -> "C:\Users", "me"; "C:Users" -> "C:Users"; "C:" -> "C:"
        if (root_len > 0) {
            const size_t end = win32_skip_name(cp, path, root_len);

            it->start = path;
            it->end = &path[end];
            it->length = end;
            it->index = 0;
            it->_next_ptr = it->end;
            return 0;
        }
    }

    // Find the start of the next path component
    while (win32_is_sep(*(it->_next_ptr))) {
        it->_next_ptr++;
    }

    if (*(it->_next_ptr) == '\0') {
        it->start = NULL;
        it->end = NULL;
        it->length = 0;
        return 1;
    }

    it->start = it->_next_ptr;
    it->index++;

    // Find the end of the next path component
    it->_next_ptr = &path[win32_skip_name(cp, path, (size_t)(it->_next_ptr - path))];
    it->end = it->_next_ptr;
    it->length = it->end - it->start;

    return 0;
}

#endif  /* _WIN32 */
