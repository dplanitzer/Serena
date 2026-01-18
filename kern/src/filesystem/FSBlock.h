//
//  FSBlock.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/27/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef FSBlock_h
#define FSBlock_h

#include <stdint.h>


typedef enum {
    kMapBlock_ReadOnly,     // Map the disk block for reading only with no write back
    kMapBlock_Update,       // Map the disk block for a partial update and write back
    kMapBlock_Replace,      // Map the disk block for a full update where every byte will be replaced and written back
    kMapBlock_Cleared       // Map the disk block with every byte cleared, potential additional full or partial updates and write back
} MapBlock;


typedef enum WriteBlock {
    kWriteBlock_None,           // Don't write the disk block back to disk
    kWriteBlock_Sync,           // Write the disk block back to disk and wait for the write to finish
    kWriteBlock_Deferred        // Mark the disk block as needing write back but wait with the write back until a flush event happens or the block is needed for another disk address
} WriteBlock;


typedef struct FSBlock {
    intptr_t            token;
    uint8_t* _Nullable  data;
} FSBlock;

#endif /* FSBlock_h */
