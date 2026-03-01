//
//  str_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 2/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <stdbool.h>
#include <ext/string.h>
#include "Asserts.h"


static bool str_equ(const char * _Nonnull lhs, const char * _Nonnull rhs)
{
    while (*lhs == *rhs) {
        if (*lhs == '\0') {
            return true;
        }
        lhs++; rhs++;
    }

    return false;
}


void str_test(int argc, char *argv[])
{
    char buf_8[8];

    // strlen
    assertEquals(0, strlen(""));
    assertEquals(3, strlen("foo"));
    assertEquals(6, strlen("foobar"));


    // strcpy
    assertEquals(buf_8, strcpy(buf_8, ""));
    assertTrue(str_equ(buf_8, ""));
    assertEquals(buf_8, strcpy(buf_8, "hello"));
    assertTrue(str_equ(buf_8, "hello"));

    // strcpy_x
    assertEquals(&buf_8[0], strcpy_x(buf_8, ""));
    assertTrue(str_equ(buf_8, ""));
    assertEquals(&buf_8[5], strcpy_x(buf_8, "hello"));
    assertTrue(str_equ(buf_8, "hello"));


    // strcat
    buf_8[0] = '\0';
    assertEquals(buf_8, strcat(buf_8, ""));
    assertTrue(str_equ(buf_8, ""));
    buf_8[0] = '1'; buf_8[1] = '\0';
    assertEquals(buf_8, strcat(buf_8, "hello"));
    assertTrue(str_equ(buf_8, "1hello"));

    // strcat_x
    buf_8[0] = '\0';
    assertEquals(&buf_8[0], strcat_x(buf_8, ""));
    assertTrue(str_equ(buf_8, ""));
    buf_8[0] = '1'; buf_8[1] = '\0';
    assertEquals(&buf_8[6], strcat_x(buf_8, "hello"));
    assertTrue(str_equ(buf_8, "1hello"));
    buf_8[0] = '1'; buf_8[1] = '\0';
    assertEquals(&buf_8[1], strcat_x(buf_8, ""));
    assertTrue(str_equ(buf_8, "1"));


    // strcmp
    assertEquals(0, strcmp("", ""));
    assertEquals(0, strcmp("abc", "abc"));
    assertEquals(-1, strcmp("abc", "abcd"));
    assertEquals(1, strcmp("abcd", "abc"));
    assertEquals(-1, strcmp("a", "b"));
    assertEquals(1, strcmp("b", "a"));


    // strchr
    strcpy(buf_8, "hello");
    assertEquals(&buf_8[0], strchr(buf_8, 'h'));
    assertEquals(&buf_8[2], strchr(buf_8, 'l'));
    assertEquals(&buf_8[5], strchr(buf_8, '\0'));
    assertEquals(NULL, strchr(buf_8, 'x'));


    // strrchr
    strcpy(buf_8, "hello");
    assertEquals(&buf_8[0], strrchr(buf_8, 'h'));
    assertEquals(&buf_8[3], strrchr(buf_8, 'l'));
    assertEquals(&buf_8[5], strrchr(buf_8, '\0'));
    assertEquals(NULL, strrchr(buf_8, 'x'));
}
