//
//  StdioTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <System/System.h>
#include "Asserts.h"


void fopen_memory_fixed_size_test(int argc, char *argv[])
{
    static char data[16];
    static char buf[16];
    FILE_Memory mem;
    FILE_MemoryQuery q;

    mem.base = data;
    mem.initialEof = 0;
    mem.initialCapacity = sizeof(data);
    mem.maximumCapacity = mem.initialCapacity;
    mem.options = 0;

    FILE* fp = fopen_memory(&mem, "rw");
    assertNotNULL(fp);
    assertNotEOF(fputs("Hello", fp));
    assertEOF(fgetc(fp));
    assertNotEOF(fseek(fp, 0, SEEK_SET));
    assertNotNULL(fgets(buf, sizeof(buf), fp));
    puts(buf);

    assertNotEOF(fputs(" World 1234", fp));
    assertNotEOF(filemem(fp, &q));
    printf("base: %#p, eof: %zu, capacity: %zu\nok\n", q.base, q.eof, q.capacity);
}

void fopen_memory_variable_size_test(int argc, char *argv[])
{
    static char buf[16];
    FILE_Memory mem;
    FILE_MemoryQuery q;

    mem.base = NULL;
    mem.initialEof = 0;
    mem.initialCapacity = 0;
    mem.maximumCapacity = 16;
    mem.options = 0;

    FILE* fp = fopen_memory(&mem, "rw");
    assertNotNULL(fp);
    assertNotEOF(fputs("Hello", fp));
    assertEOF(fgetc(fp));
    assertNotEOF(fseek(fp, 0, SEEK_SET));
    assertNotNULL(fgets(buf, sizeof(buf), fp));
    puts(buf);

    assertNotEOF(fputs(" World 1234", fp));
    assertNotEOF(filemem(fp, &q));
    printf("base: %#p, eof: %zu, capacity: %zu\nok\n", q.base, q.eof, q.capacity);
}
