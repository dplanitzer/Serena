//
//  mem_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/10/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "Asserts.h"


static bool has_value(char* p, char ch, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (*p++ != ch) {
            return false;
        }
    }

    return true;
}


void mem_test(int argc, char *argv[])
{
    const size_t MEMBLK_SIZE = 32*1024;
    char* p = calloc(MEMBLK_SIZE, 1);

    // memset, aligned p, 1 byte
    memset(p + 16, 0xaa, 1);
    assertTrue(has_value(p, 0, 16));
    assertTrue(has_value(p + 16, 0xaa, 1));
    assertTrue(has_value(p + 16 + 1, 0, 16));
    memset(p, 0, MEMBLK_SIZE);


    // memset, aligned p, 33 byte
    memset(p + 16, 0xaa, 33);
    assertTrue(has_value(p, 0, 16));
    assertTrue(has_value(p + 16, 0xaa, 33));
    assertTrue(has_value(p + 16 + 33, 0, 16));
    memset(p, 0, MEMBLK_SIZE);


    // memset, aligned p, 64 byte
    memset(p + 16, 0xaa, 64);
    assertTrue(has_value(p, 0, 16));
    assertTrue(has_value(p + 16, 0xaa, 64));
    assertTrue(has_value(p + 16 + 64, 0, 16));
    memset(p, 0, MEMBLK_SIZE);


    // memset, UNaligned p, 1 byte
    memset(p + 15, 0xaa, 1);
    assertTrue(has_value(p, 0, 15));
    assertTrue(has_value(p + 15, 0xaa, 1));
    assertTrue(has_value(p + 15 + 1, 0, 15));
    memset(p, 0, MEMBLK_SIZE);


    // memset, UNaligned p, 33 byte
    memset(p + 15, 0xaa, 33);
    assertTrue(has_value(p, 0, 15));
    assertTrue(has_value(p + 15, 0xaa, 33));
    assertTrue(has_value(p + 15 + 33, 0, 15));
    memset(p, 0, MEMBLK_SIZE);


    // memset, UNaligned p, 64 byte
    memset(p + 15, 0xaa, 64);
    assertTrue(has_value(p, 0, 15));
    assertTrue(has_value(p + 15, 0xaa, 64));
    assertTrue(has_value(p + 15 + 64, 0, 15));
    memset(p, 0, MEMBLK_SIZE);

    free(p);
}
