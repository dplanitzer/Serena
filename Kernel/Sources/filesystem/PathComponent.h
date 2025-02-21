//
//  PathComponent.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PathComponent_h
#define PathComponent_h

#include <klib/klib.h>


// Describes a single component (name) of a path. A path is a sequence of path
// components separated by a '/' character. Note that a path component is not a
// NUL terminated string. The length of the component is given explicitly by the
// count field.
typedef struct PathComponent {
    const char* _Nonnull    name;
    ssize_t                 count;
} PathComponent;


// Path component representing "."
extern const PathComponent kPathComponent_Self;

// Path component representing ".."
extern const PathComponent kPathComponent_Parent;

// Initializes a path component from a NUL-terminated string
extern PathComponent PathComponent_MakeFromCString(const char* _Nonnull pCString);

// Returns true if the given path component is equal to the given nul-terminated
// string.
extern bool PathComponent_EqualsCString(const PathComponent* pc, const char* rhs);

// Returns true if the given path component is equal to the given string with
// the given length.
extern bool PathComponent_EqualsString(const PathComponent* pc, const char* rhs, size_t rhsLength);


// Mutable version of PathComponent. 'count' must be set on return to the actual
// length of the generated/edited path component. 'capacity' is the maximum length
// that the path component may take on.
typedef struct MutablePathComponent {
    char* _Nonnull  name;
    ssize_t         count;
    ssize_t         capacity;
} MutablePathComponent;

// Returns true if the given path component is equal to the given nul-terminated
// string.
#define MutablePathComponent_EqualsCString(__pc, __rhs) \
    PathComponent_EqualsCString((const PathComponent*)(__pc), __rhs)

// Returns true if the given path component is equal to the given string with
// the given length.
#define MutablePathComponent_EqualsString(__pc, __rhs, __rhsLength) \
    PathComponent_EqualsString((const PathComponent*)(__pc), __rhs, __rhsLength)

// Replaces the current string contents of 'pc' with the string 'str' of length
// 'len'. The string characters are copied into the character buffer of the
// receiver. ERANGE is returned if the buffer is not big enough to hold 'len'
// characters.
extern errno_t MutablePathComponent_SetString(MutablePathComponent* _Nonnull pc, const char* _Nonnull str, size_t len);

#endif /* PathComponent_h */
