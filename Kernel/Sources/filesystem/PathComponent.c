//
//  PathComponent.c
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PathComponent.h"
#include <kern/string.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Path Component
////////////////////////////////////////////////////////////////////////////////

const PathComponent kPathComponent_Self = {".", 1};
const PathComponent kPathComponent_Parent = {"..", 2};

// Initializes a path component from a NUL-terminated string
PathComponent PathComponent_MakeFromCString(const char* _Nonnull pCString)
{
    PathComponent pc;

    pc.name = pCString;
    pc.count = String_Length(pCString);
    return pc;
}

// Returns true if the given path component is equal to the given nul-terminated
// string.
bool PathComponent_EqualsCString(const PathComponent* _Nonnull pc, const char* _Nonnull rhs)
{
    const char* lhs = pc->name;
    int n = pc->count;

    if (rhs[pc->count] != '\0') {
        return false;
    }

    while (*rhs && n > 0) {
        if (*rhs != *lhs) {
            return false;
        }

        n--; lhs++; rhs++;
    }

    return (*rhs == '\0' && n == 0) ? true : false;
}

bool PathComponent_EqualsString(const PathComponent* _Nonnull pc, const char* _Nonnull rhs, size_t rhsLength)
{
    size_t lhsLength = pc->count;
    const char* lhs = pc->name;
    int n = pc->count;

    if (lhsLength != rhsLength) {
        return false;
    }

    while (lhsLength > 0 && *lhs == *rhs) {
        lhs++; rhs++; lhsLength--;
    }

    return (lhsLength == 0) ? true : false;
}

errno_t MutablePathComponent_SetString(MutablePathComponent* _Nonnull pc, const char* _Nonnull str, size_t len)
{
    if (pc->capacity < len) {
        pc->count = 0;
        return ERANGE;
    }

    pc->count = len;

    char* dp = pc->name;
    while (len-- > 0) {
        *dp++ = *str++;
    }

    return EOK;
}
