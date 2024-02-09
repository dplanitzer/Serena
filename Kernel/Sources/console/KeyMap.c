//
//  KeyMap.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/8/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "KeyMap.h"


// Returns true if the given key map is valid and false otherwise.
// XXX Should check that all offsets are inside the pMap->size range.
bool KeyMap_IsValid(const KeyMap* _Nonnull pMap)
{
    const uint8_t* pMapBase = (const uint8_t*) pMap;

    for (uint16_t r = 0; r < pMap->rangeCount; r++) {
        const KeyMapRange* pCurRange = (const KeyMapRange*)(pMapBase + pMap->rangeOffset[r]);
        const uint16_t keyCodeCount = pCurRange->upper - pCurRange->lower;

        switch (pCurRange->type) {
            case KEY_MAP_RANGE_TYPE_3: {
                const uint16_t* pCurTraps = (const uint16_t*)(pMapBase + pCurRange->traps);

                for (uint16_t k = 0; k <= keyCodeCount; k++) {
                    // XXX This should really only check at most kKeyMap_MaxByteSequenceLength+1 bytes
                    if (String_Length((const char*)(pMapBase + pCurTraps[k])) > kKeyMap_MaxByteSequenceLength) {
                        return false;
                    }
                }
            } break;

            default:
                break;
        }
    }
    return true;
}



static ssize_t KeyMapRange_Type0_Map(const KeyMapRange* _Nonnull pRange, const uint8_t* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, char* _Nonnull pOutChars)
{
    uint32_t evtFlags = pEvent->flags;

    if ((evtFlags & kHIDEventModifierFlag_CapsLock) && (pEvent->keyCode >= KEY_A && pEvent->keyCode <= KEY_Z)) {
        // Treat as shift for caps-able USB key code, except if the shift key is pressed at the same time
        if (!(evtFlags & kHIDEventModifierFlag_Shift)) {
            evtFlags |= kHIDEventModifierFlag_Shift;
        } else {
            evtFlags &= ~kHIDEventModifierFlag_Shift;
        }
    }

    const uint32_t* pTraps = (const uint32_t*)(pMapBase + pRange->traps);
    const char* trap = (const char*) &pTraps[pEvent->keyCode - pRange->lower];
    const unsigned int modifierIdx = evtFlags & (kHIDEventModifierFlag_Shift|kHIDEventModifierFlag_Option);
    char ch = trap[modifierIdx];

    if (ch == '\0') {
        ch = trap[0];
    }
    if (evtFlags & kHIDEventModifierFlag_Control) {
        ch &= 0x1f;     // drop bits 7, 6 and 5
    }

    *pOutChars = ch;

    return 1;
}

// Maps a USB key code to a nul-terminated UTF8 string. Ignores modifier keys.
static ssize_t KeyMapRange_Type3_Map(const KeyMapRange* _Nonnull pRange, const uint8_t* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, char* _Nonnull pOutChars, ssize_t maxOutChars)
{
    const uint16_t* pTrapOffsets = (const uint16_t*)(pMapBase + pRange->traps);
    const int adjustedUsbKeyCode = pEvent->keyCode - pRange->lower;
    const uint16_t targetTrapOffset = pTrapOffsets[adjustedUsbKeyCode];
    const char* pTrapString = (const char*)(pMapBase + targetTrapOffset);
    ssize_t i = 0;
    
    while (*pTrapString != '\0' && i < maxOutChars) {
        *pOutChars++ = *pTrapString++;
        i++;
    }

    return i;
}


// Maps the given up/down key event to a sequence of bytes. Usually that
// sequence is only a single byte long. However it may be multiple bytes
// long or of length 0. The length of the sequence the event was mapped to is
// returned. If that length is zero then the key press or release should be
// ignored. Note that this function returns a sequence of bytes and not a
// C string. Consequently the sequence is not nul-terminated.
ssize_t KeyMap_Map(const KeyMap* _Nonnull pMap, const HIDEventData_KeyUpDown* _Nonnull pEvent, void* _Nonnull pBuffer, ssize_t maxOutBytes)
{
    if (maxOutBytes > 0) {
        const uint8_t* pMapBase = (const uint8_t*)pMap;
        const HIDKeyCode usbKeyCode = pEvent->keyCode;

        for (int i = 0; i < pMap->rangeCount; i++) {
            const KeyMapRange* pCurRange = (const KeyMapRange*)(pMapBase + pMap->rangeOffset[i]);

            if (usbKeyCode >= pCurRange->lower && usbKeyCode <= pCurRange->upper) {
                switch (pCurRange->type) {
                    case KEY_MAP_RANGE_TYPE_0:
                        return KeyMapRange_Type0_Map(pCurRange, pMapBase, pEvent, pBuffer);

                    case KEY_MAP_RANGE_TYPE_3:
                        return KeyMapRange_Type3_Map(pCurRange, pMapBase, pEvent, pBuffer, maxOutBytes);

                    default:
                        return 0;
                }
            }
        }
    }

    return 0;
}
