//
//  stdbool.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _STDBOOL_H
#define _STDBOOL_H 1

// Note that a boolean value of true must have bit #0 set. All other bits may or
// may not be set in addition to bit #0. The reason for this design choice is
// that by requiring only one bit to be set for a true value that we can take
// advantage of CPU bit set/get instructions to implement an atomic boolean set
// operation that can return the old value of the boolean. That way we do not
// need to turn off interrupts while manipulating the atomic bool. Functions
// which accept boolean values as input should check for ==0 (false) and != 0
// (true) instead of checking for a specific bit pattern to detect a true value.
// Functions which return a boolean value should return the canonical values
// defined below.
//typedef unsigned char _Bool;
#ifndef __bool_true_false_are_defined

#define bool unsigned char //_Bool
#define true    1
#define false   0
#define __bool_true_false_are_defined 1
#endif

#endif /* _STDBOOL_H */
