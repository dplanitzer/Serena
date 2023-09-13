//
//  KeyMap.h
//  Apollo
//
//  Created by Dietmar Planitzer on 9/8/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef KeyMap_h
#define KeyMap_h

#include <klib/klib.h>
#include "HIDEvent.h"
#include "USBHIDKeys.h"

// A positive byte offset to the desired data. The offset is relative to the
// first byte of the KeyMap data structure.
typedef UInt16  KeyMapOffset;


// 'traps' points to an array of 4 bytes per key:
// *) byte #0: unmodified, byte#1: shift, byte #2: option, byte #3: shift + option
// *) control key clears bits 7, 6 & 5 and is applied after applying the shift
//    and alt keys to select the character
// *) left and right hand modifier keys produce the same character
// *) a \0 character in one of the shifted positions produces the base character
// *) a \0 character is not allowed in the base position
// *) all keys are marking
#define KEY_MAP_RANGE_TYPE_0    0

// XXX Consider adding TYPE_0 variants for 16bit and 32bit per shift state

// 'traps' points to an array of 16 bit key map offsets. Each offset points to
// a nul-terminated UTF8 character string:
// *) all modifier keys are ignored and map to the base case
// *) an empty string is not allow
// *) all keys are marking
#define KEY_MAP_RANGE_TYPE_3    3

// XXX Consider adding TYPE_3 variant that stores a string up to 4 bytes in 32bits inline

typedef struct _KeyMapRange {
    UInt16          type;
    HIDKeyCode      lower;
    HIDKeyCode      upper;
    KeyMapOffset    traps;
} KeyMapRange;


// Big endian
#define KEY_MAP_TYPE_0  0

typedef struct _KeyMap {
    UInt16          type;
    UInt16          size;   // Overall size of key map in bytes
    UInt16          rangeCount;
    KeyMapOffset    rangeOffset[1];
} KeyMap;


// XXX Should have something like a KeyMap_Validate() that checks that all offsets
// XXX are inside the pMap->size range

// XXX Should have a function that calculates the maximum size that we need for
// XXX 'pOutChars'

extern ByteCount KeyMap_Map(const KeyMap* _Nonnull pMap, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars, ByteCount maxOutChars);

#endif /* KeyMap_h */
