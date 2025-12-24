//
//  environ.c
//  libc
//
//  Created by Dietmar Planitzer on 9/15/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ext/string.h>
#include <__stddef.h>

// XXX We currently leak environment table entries because of the broken putenv() semantics

// Returns the number of entries in the environment table. Does not include the
// terminating NULL entry.
static ssize_t __getenvsize(void)
{
    char** p = environ;

    while (*p++);
    return p - environ - 1;
}

// Like getenv() but also returns the index of the 'name' entry in the environ
// table. Returns NULL and an index of -1 if 'name' was not found. Note that at
// most 'nMaxChars' are considered when comparing 'name' against the entries
// stored in the table.
static char* _Nullable __getenv(const char *_Nonnull name, size_t nMaxChars, ssize_t* _Nonnull idx)
{
    if (name) {
        const char* const * p = (const char * const *) environ;

        while (*p) {
            const char* vname = *p;

            if (!strncmp(name, vname, nMaxChars) && vname[nMaxChars] == '=') {
                *idx = p - environ;
                return (char*) &vname[nMaxChars + 1];
            }
            p++;
        }
    }

    *idx = -1;
    return NULL;
}

// Replaces the entry at the given index in the environment table with the given
// entry.
static void __putenvat(char* _Nonnull entry, ssize_t idx)
{
    environ[idx] = entry;
}

// Appends the given entry to the environment table. The table is expanded if
// it is full.
static int __addenv(char* _Nonnull entry)
{
    const ssize_t oldSize = __getenvsize();
    char** pDst = (char**) malloc(sizeof(char*) * (oldSize + 2));

    if (pDst == NULL) {
        return -1;
    }

    char** pSrc = environ;
    while (*pSrc) {
        *pDst++ = *pSrc++;
    }
    *pDst++ = entry;
    *pDst = NULL;

    if (!__is_pointer_NOT_freeable(environ)) {
        free(environ);
    }

    environ = pDst;
    
    return 0;
}

// Like unsetenv() except that it does not validate 'name'. The search for 'name'
// starts at the entry with index 'startIdx'. Note that the function does not
// validate the index. Note that at most 'nMaxChars' are considered when
// comparing 'name' against the entries stored in the table.
static void __unsetenv(const char * _Nonnull name, size_t nMaxChars, ssize_t startIdx)
{
    char** p = &environ[startIdx];

    // Keep in mind that a name may appear more than once in the environment
    while (*p) {
        const char* vname = *p;

        // Allow a user to unset a broken entry that has no value. Eg "bla" instead of "bla=foo".
        if (!strncmp(name, vname, nMaxChars) && (vname[nMaxChars] == '=' || vname[nMaxChars] == '\0')) {
            char** cp = p;

            while (*cp) {
                *cp++ = *cp;
            }
        } else {
            p++;
        }
    }
}

// Creates a environment conforming key-value pair of the form 'name=value'. The
// key-value paired is malloc()'d.
static char* _Nullable __createenventry(const char* _Nonnull name, const char* _Nonnull value)
{
    char* p = (char*) malloc(strlen(name) + strlen(value) + 2);

    if (p) {
        strcat_x(strcat_x(strcpy_x(p, name), "="), value);
    }
    return p;
}



char *getenv(const char *name)
{
    ssize_t idx;
    return __getenv(name, strlen(name), &idx);
}

int setenv(const char *name, const char *value, int overwrite)
{
    if (name == NULL || *name == '\0' || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }


    // Check whether at least one entry with 'name' exists
    const size_t nameLen = strlen(name);
    ssize_t idxOfOldEntry;
    (void) __getenv(name, nameLen, &idxOfOldEntry);


    // Done if changing the entry isn't allow (useless feature that is a duplication of calling getenv() yourself)
    if (overwrite == 0 && idxOfOldEntry >= 0) {
        return 0;
    }


    if (idxOfOldEntry >= 0) {
        // 'name' exists in the table at least once.
        
        // If 'value' doesn't exist then remove all occurrences of 'name' and
        // return.
        if (value == NULL || *value == '\0') {
            __unsetenv(name, nameLen, idxOfOldEntry);
            return 0;
        }


        // Replace the entry that we found and then remove all old duplicate
        // entries of 'name'
        char* entry = __createenventry(name, value);
        if (entry == NULL) {
            return -1;
        }

        __putenvat(entry, idxOfOldEntry);
        __unsetenv(name, nameLen, idxOfOldEntry + 1);
    } else {
        // 'name' doesn't exist in the table. Add it
        char* entry = __createenventry(name, value);

        if (entry == NULL) {
            return -1;
        } 
        return __addenv(entry);
    }
}

int putenv(char *str)
{
    if (str == NULL || *str == '\0') {
        return -1;
    }

    // Just remove the entry if there's no value associated with it
    char* sep = strrchr(str, '=');
    if (sep == NULL) {
        __unsetenv(str, strlen(str), 0);
        return 0;
    }


    // Frankly, we don't care about performance here. In fact slow is good because
    // the design of this API is broken beyond repair. It's not concurrency safe
    // and there's no easy way to free strings added by this function because we
    // don't know who owns 'str' and what the lifetime of the string is.
    // People who don't like this should fix their code and stop relying on trash
    // as an API.

    // Remove all occurrences of 'name' and add the new entry
    const char* name = str;
    const size_t nameLen = sep - name;
    __unsetenv(name, nameLen, 0);
    __addenv(str);

    return 0;
}

int unsetenv(const char *name)
{
    if (name == NULL || *name == '\0' || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    __unsetenv(name, strlen(name), 0);
    return 0;
}
