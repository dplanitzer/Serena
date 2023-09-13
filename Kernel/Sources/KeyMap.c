//
//  KeyMap.c
//  Apollo
//
//  Created by Dietmar Planitzer on 9/8/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "KeyMap.h"


static ByteCount KeyMapRange_Type0_Map(const KeyMapRange* _Nonnull pRange, const Byte* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars)
{
    const UInt32* pTraps = (const UInt32*)(pMapBase + pRange->traps);
    const Int adjustedUsbKeyCode = pEvent->keycode - pRange->lower;
    const UInt modifierIdx = pEvent->flags & (kHIDEventModifierFlag_Shift|kHIDEventModifierFlag_Option);
    const Character* trap = (const Character*) &pTraps[adjustedUsbKeyCode];
    Character ch = trap[modifierIdx];

    if (ch == '\0') {
        ch = trap[0];
    }
    if (pEvent->flags & kHIDEventModifierFlag_Control) {
        ch &= 0x1f;     // drop bits 7, 6 and 5
    }

    *pOutChars = ch;

    return 1;
}

// Maps a USB key code to a nul-terminated UTF8 string. Ignores modifier keys.
static ByteCount KeyMapRange_Type3_Map(const KeyMapRange* _Nonnull pRange, const Byte* _Nonnull pMapBase, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars, ByteCount maxOutChars)
{
    const UInt16* pTrapOffsets = (const UInt16*)(pMapBase + pRange->traps);
    const Int adjustedUsbKeyCode = pEvent->keycode - pRange->lower;
    const UInt16 targetTrapOffset = pTrapOffsets[adjustedUsbKeyCode];
    const Character* pTrapString = (const Character*)(pMapBase + targetTrapOffset);
    ByteCount i = 0;
    
    while (*pTrapString != '\0' && i < maxOutChars) {
        *pOutChars++ = *pTrapString++;
        i++;
    }

    return i;
}


ByteCount KeyMap_Map(const KeyMap* _Nonnull pMap, const HIDEventData_KeyUpDown* _Nonnull pEvent, Character* _Nonnull pOutChars, ByteCount maxOutChars)
{
    if (pMap->type == KEY_MAP_TYPE_0 && maxOutChars > 0) {
        const Byte* pMapBase = (const Byte*)pMap;
        const HIDKeyCode usbKeyCode = pEvent->keycode;

        for (Int i = 0; i < pMap->rangeCount; i++) {
            const KeyMapRange* pCurRange = (const KeyMapRange*)(pMapBase + pMap->rangeOffset[i]);

            if (usbKeyCode >= pCurRange->lower && usbKeyCode <= pCurRange->upper) {
                switch (pCurRange->type) {
                    case KEY_MAP_RANGE_TYPE_0:
                        return KeyMapRange_Type0_Map(pCurRange, pMapBase, pEvent, pOutChars);

                    case KEY_MAP_RANGE_TYPE_3:
                        return KeyMapRange_Type3_Map(pCurRange, pMapBase, pEvent, pOutChars, maxOutChars);

                    default:
                        return 0;
                }
            }
        }
    }

    return 0;
}
