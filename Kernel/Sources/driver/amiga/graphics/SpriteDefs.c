//
//  SpriteDefs.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/25/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <klib/klib.h>

#define PACK_U16(_15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, _3, _2, _1, _0) \
    (uint16_t)(((_15) << 15) | ((_14) << 14) | ((_13) << 13) | ((_12) << 12) | ((_11) << 11) |\
             ((_10) << 10) |  ((_9) <<  9) |  ((_8) <<  8) |  ((_7) <<  7) |  ((_6) <<  6) |\
              ((_5) <<  5) |  ((_4) <<  4) |  ((_3) <<  3) |  ((_2) <<  2) |  ((_1) <<  1) | (_0))

#define _ 0
#define o 1

#if 0
#define ARROW_BITS_PLANE0 \
PACK_U16( o,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,o,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,o,_,_,_,_,_,_,_ ),
const uint16_t gArrow_Plane0[] = {
    ARROW_BITS_PLANE0
};

#define ARROW_BITS_PLANE1 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,o,_,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,o,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
const uint16_t gArrow_Plane1[] = {
    ARROW_BITS_PLANE1
};
#endif
// XXX designed for use by the mouse painter. Not as a sprite.
#define ARROW_BITS \
PACK_U16( o,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,_,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,_,_,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,_,_,_,o,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,_,_,_,_,o,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,_,o,o,o,o,o,_,_,_,_,_ ), \
PACK_U16( o,_,_,o,_,_,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,o,_,o,_,_,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,o,_,_,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,_,_,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,_,_,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,o,_,_,_,_,_,_,_ ),
const uint16_t gArrow_Bits[] = {
    ARROW_BITS
};

#define ARROW_MASK \
PACK_U16( o,o,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,o,o,o,o,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,o,o,o,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,_,_,o,o,o,o,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,o,o,o,o,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,o,o,o,_,_,_,_,_,_,_ ),
const uint16_t gArrow_Mask[] = {
    ARROW_MASK
};

const int gArrow_Width = 16;
const int gArrow_Height = 16;


#define BLOCK_4x8_BITS_PLANE0 \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ),
const uint16_t gBlock4x8_Plane0[] = {
    BLOCK_4x8_BITS_PLANE0
};

#define BLOCK_4x8_BITS_PLANE1 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
const uint16_t gBlock4x8_Plane1[] = {
    BLOCK_4x8_BITS_PLANE1
};

const int gBlock4x8_Width = 16;
const int gBlock4x8_Height = 8;


#define BLOCK_4x4_BITS_PLANE0 \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( o,o,o,o,_,_,_,_,_,_,_,_,_,_,_,_ ),
const uint16_t gBlock4x4_Plane0[] = {
    BLOCK_4x4_BITS_PLANE0
};

#define BLOCK_4x4_BITS_PLANE1 \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ), \
PACK_U16( _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_ ),
const uint16_t gBlock4x4_Plane1[] = {
    BLOCK_4x4_BITS_PLANE1
};

const int gBlock4x4_Width = 16;
const int gBlock4x4_Height = 4;
