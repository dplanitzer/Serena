//
//  ConsoleChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef ConsoleChannel_h
#define ConsoleChannel_h

#include <IOChannel.h>
#include "KeyMap.h"


// Big enough to hold the result of a key mapping and the longest possible
// terminal report message.
#define MAX_MESSAGE_LENGTH kKeyMap_MaxByteSequenceLength

// The console I/O channel
//
// Takes care of mapping a USB key scan code to a character or character sequence.
// We may leave partial character sequences in the buffer if a Console_Read() didn't
// read all bytes of a sequence. The next Console_Read() will first receive the
// remaining buffered bytes before it receives bytes from new events.
open_class_with_ref(ConsoleChannel, IOChannel,
    ObjectRef   console;
    char        rdBuffer[MAX_MESSAGE_LENGTH];   // Holds a full or partial byte sequence produced by a key down event
    int8_t      rdCount;                        // Number of bytes stored in the buffer
    int8_t      rdIndex;                        // Index of first byte in the buffer where a partial byte sequence begins
);
open_class_funcs(ConsoleChannel, IOChannel,
);


extern errno_t ConsoleChannel_Create(ObjectRef _Nonnull pConsole, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* ConsoleChannel_h */
