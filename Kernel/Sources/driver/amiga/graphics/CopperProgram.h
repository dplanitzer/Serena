//
//  CopperProgram.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef CopperProgram_h
#define CopperProgram_h

#include <klib/Error.h>
#include <klib/List.h>
#include <klib/Types.h>


// Copper instructions
typedef uint32_t  CopperInstruction;

#define COP_MOVE(reg, val)  (((reg) << 16) | (val))
#define COP_END()           0xfffffffe


typedef struct CopperProgram {
    SListNode           node;
    CopperInstruction   entry[1];
} CopperProgram;


extern errno_t CopperProgram_Create(size_t instrCount, CopperProgram* _Nullable * _Nonnull pOutSelf);
extern void CopperProgram_Destroy(CopperProgram* _Nullable self);

#endif /* CopperProgram_h */
