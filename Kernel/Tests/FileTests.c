//
//  FileTests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 11/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <ctype.h>
#include <stdbool.h>
#define _OPEN_SYS_ITOA_EXT
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Asserts.h"


void overwrite_file_test(int argc, char *argv[])
{
    FILE* fp = NULL;

    printf("overwrite: /Users/Administrator/while.sh\n");

    fp = fopen("/Users/Administrator/while.sh", "rb+");
    assertNotNULL(fp);
    
    setvbuf(fp, NULL, _IONBF, 0);

    const char* str = "HELLO";
    const size_t r = fwrite(str, strlen(str), 1, fp);
    assertEquals(1, r);

    fclose(fp);
}
