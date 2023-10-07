//
//  KeyMap.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/8/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "KeyMap.h"


// Returns the maximum size of the output buffer that is needed for the
// KeyMap_Map() function.
ByteCount KeyMap_GetMaxOutputByteCount(const KeyMap* _Nonnull pMap)
{
    const Byte* pMapBase = (const Byte*) pMap;
    ByteCount maxOutChars = 0;

    for (UInt16 r = 0; r < pMap->rangeCount; r++) {
        const KeyMapRange* pCurRange = (const KeyMapRange*)(pMapBase + pMap->rangeOffset[r]);
        const UInt16 keyCodeCount = pCurRange->upper - pCurRange->lower;

        switch (pCurRange->type) {
            case KEY_MAP_RANGE_TYPE_3: {
                const UInt16* pCurTraps = (const UInt16*)(pMapBase + pCurRange->traps);

                for (UInt16 k = 0; k <= keyCodeCount; k++) {
                    maxOutChars = __max(maxOutChars, String_Length((const char*)(pMapBase + pCurTraps[k])));
                }
            } break;

            default:
                break;
        }
    }
    return maxOutChars;
}



static ByteCount KeyMapRange_Type0_Map(const KeyMapRange* _Nonnull pRange, const Byte* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars)
{
    UInt32 evtFlags = pEvent->flags;

    if ((evtFlags & kHIDEventModifierFlag_CapsLock) && (pEvent->keyCode >= KEY_A && pEvent->keyCode <= KEY_Z)) {
        // Treat as shift for caps-able USB key code, except if the shift key is pressed at the same time
        if (!(evtFlags & kHIDEventModifierFlag_Shift)) {
            evtFlags |= kHIDEventModifierFlag_Shift;
        } else {
            evtFlags &= ~kHIDEventModifierFlag_Shift;
        }
    }

    const UInt32* pTraps = (const UInt32*)(pMapBase + pRange->traps);
    const Character* trap = (const Character*) &pTraps[pEvent->keyCode - pRange->lower];
    const UInt modifierIdx = evtFlags & (kHIDEventModifierFlag_Shift|kHIDEventModifierFlag_Option);
    Character ch = trap[modifierIdx];

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
static ByteCount KeyMapRange_Type3_Map(const KeyMapRange* _Nonnull pRange, const Byte* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars, ByteCount maxOutChars)
{
    const UInt16* pTrapOffsets = (const UInt16*)(pMapBase + pRange->traps);
    const Int adjustedUsbKeyCode = pEvent->keyCode - pRange->lower;
    const UInt16 targetTrapOffset = pTrapOffsets[adjustedUsbKeyCode];
    const Character* pTrapString = (const Character*)(pMapBase + targetTrapOffset);
    ByteCount i = 0;
    
    while (*pTrapString != '\0' && i < maxOutChars) {
        *pOutChars++ = *pTrapString++;
        i++;
    }

    return i;
}


// Maps the given up/down key event to a sequence of bytes. Usually that
// sequence is only a single byte long. However it may be multiple bytes
// long or of length 0. The length of teh sequence the event was mapped to is
// returned. If that length is zero then the key press or release should be
// ignored. Note that this function returns a sequence of bytes and not a
// C string. Consequently the sequence is not nul-terminated.
ByteCount KeyMap_Map(const KeyMap* _Nonnull pMap, const HIDEventData_KeyUpDown* _Nonnull pEvent, Byte* _Nonnull pBuffer, ByteCount maxOutBytes)
{
    if (maxOutBytes > 0) {
        const Byte* pMapBase = (const Byte*)pMap;
        const HIDKeyCode usbKeyCode = pEvent->keyCode;

        for (Int i = 0; i < pMap->rangeCount; i++) {
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
