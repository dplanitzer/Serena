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

// A key map maps a USB key code to a single character or a string. The mapping
// can take the current state of the modifier flags into account.
//
// A key map consists of a table of 'key mapping ranges'. Each key mapping range
// starts at a certain USB key code and covers all USB key codes up to an upper
// boundary key code. Each key code range consists of a table of 'key code traps'.
//
// There are different types of key code traps: some map a USB key code to a
// single character depending on the current state of the modifier flags and
// others map a single USB key code to a string independent of the current state
// of the modifier flags.
//
// Additional key code trap types may be introduced in the future.
//
// Each key code trap has an associated action. Currently the only supported
// action is the default action which is 'marking'.
//
// The key code trap actions are:
// *) Marking: the associated character is returned
// *) Combining: the associated character is added to a buffer. This action
//               continues until the user presses a key that is associated with
//               a marking character. The marking character is still appended to
//               the buffer and the buffer is drained and its contents returned.
//
// The combining action gives the same functionality that dead keys gave in the
// traditional model except that they make it easier to define dead keys since
// dead keys naturally map to decomposed Unicode character strings.


// A positive byte offset to the desired data. The offset is relative to the
// first byte of the KeyMap data structure.
typedef uint16_t  KeyMapOffset;


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
    uint16_t        type;
    HIDKeyCode      lower;
    HIDKeyCode      upper;
    KeyMapOffset    traps;
} KeyMapRange;


// Big endian
#define KEY_MAP_TYPE_0  0

typedef struct _KeyMap {
    uint16_t        type;
    uint16_t        size;   // Overall size of key map in bytes
    uint16_t        rangeCount;
    KeyMapOffset    rangeOffset[1];
} KeyMap;


// Longest possible byte sequence that a key can produce and that KeyMap_Map()
// will return.
// The max length is chosen such that a single key stroke can be mapped to
// 4 UTF-32 characters.
#define kKeyMap_MaxByteSequenceLength   16


// Returns true if the given key map is valid and false otherwise.
// XXX Should check that all offsets are inside the pMap->size range.
extern bool KeyMap_IsValid(const KeyMap* _Nonnull pMap);

// Maps the given up/down key event to a sequence of bytes. Usually that
// sequence is only a single byte long. However it may be multiple bytes
// long or of length 0. The length of the sequence the event was mapped to is
// returned. If that length is zero then the key press or release should be
// ignored. Note that this function returns a sequence of bytes and not a
// C string. Consequently the sequence is not nul-terminated.
extern ssize_t KeyMap_Map(const KeyMap* _Nonnull pMap, const HIDEventData_KeyUpDown* _Nonnull pEvent, void* _Nonnull pBuffer, ssize_t maxOutBytes);

#endif /* KeyMap_h */
