//
//  scanf_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 3/2/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <limits.h>
#include <ext/stdlib.h>
#include <string.h>
#include "Asserts.h"


void scanf_test(int argc, char *argv[])
{
    char ch[8];
    int ival, ival2;
    double dval;

    assert_int_eq(0, sscanf("hello", "hello"));
    assert_int_eq(0, sscanf("hello", ""));
    assert_int_eq(EOF, sscanf("", "hello"));

    assert_int_eq(0, sscanf("hello world", "hello world"));
    assert_int_eq(0, sscanf("hello   \t\n\r\v world", "hello world"));
    assert_int_eq(0, sscanf("hello   \t\n world", "hello \t\n\r\v world"));

    assert_int_eq(1, sscanf("\t world", " %c", &ch[0]));
    assert_char_eq('w', ch[0]);

    assert_int_eq(1, sscanf("\t world", " %2c", ch));
    assert_char_eq('w', ch[0]);
    assert_char_eq('o', ch[1]);

    assert_int_eq(1, sscanf("\t world", "%2s", ch));
    assert_char_eq('w', ch[0]);
    assert_char_eq('o', ch[1]);
    assert_char_eq('\0', ch[2]);

    assert_int_eq(1, sscanf("\t wo  ", "%s", ch));
    assert_char_eq('w', ch[0]);
    assert_char_eq('o', ch[1]);
    assert_char_eq('\0', ch[2]);

    assert_int_eq(1, sscanf(" 12hello", "%[0-9 ]hello", ch));
    assert_char_eq(' ', ch[0]);
    assert_char_eq('1', ch[1]);
    assert_char_eq('2', ch[2]);
    assert_char_eq('\0', ch[3]);

    assert_int_eq(1, sscanf("a1 ", "%[a1 ]hello", ch));
    assert_char_eq('a', ch[0]);
    assert_char_eq('1', ch[1]);
    assert_char_eq(' ', ch[2]);
    assert_char_eq('\0', ch[3]);

    assert_int_eq(1, sscanf("bc", "%[^a]hello", ch));
    assert_char_eq('b', ch[0]);
    assert_char_eq('c', ch[1]);
    assert_char_eq('\0', ch[2]);

    assert_int_eq(2, sscanf("-123:  243", "%d:%d", &ival, &ival2));
    assert_int_eq(-123, ival);
    assert_int_eq(243, ival2);

    assert_int_eq(1, sscanf("0b111", "%i", &ival));
    assert_int_eq(7, ival);
    assert_int_eq(1, sscanf("0xabc", "%i", &ival));
    assert_int_eq(0xabc, ival);
    assert_int_eq(1, sscanf("0777", "%i", &ival));
    assert_int_eq(0777, ival);
    assert_int_eq(1, sscanf("0877", "%i", &ival));   // parsing needs to stop after 0
    assert_int_eq(0, ival);

    assert_int_eq(1, sscanf("3.1415", "%lf", &dval));
    assert_double_eq(3.1415, dval);

    assert_int_eq(1, sscanf("1e3", "%lf", &dval));
    assert_double_eq(1000.0, dval);
}
