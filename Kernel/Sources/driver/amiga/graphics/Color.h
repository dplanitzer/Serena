//
//  Color.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/24/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Color_h
#define Color_h

#include <klib/Types.h>


typedef enum ColorType {
    kColorType_RGB32,
    kColorType_Index
} ColorType;


// A RGB color
typedef uint32_t RGBColor32;


// A color type
typedef struct Color {
    ColorType   tag;
    union {
        RGBColor32  rgb32;
        int         index;
    }           u;
} Color;


// Returns a packed 32bit RGB color value
#define RGBColor32_Make(__r, __g, __b) \
    ((((__r) & 0xff) << 16) | (((__g) & 0xff) << 8) | ((__b) & 0xff))

// Returns the red component of a RGB32
#define RGBColor32_GetRed(__clr) \
    (((__clr) >> 16) & 0xff)

// Returns the green component of a RGB32
#define RGBColor32_GetGreen(__clr) \
    (((__clr) >> 8) & 0xff)

// Returns the blue component of a RGB32
#define RGBColor32_GetBlue(__clr) \
    ((__clr) & 0xff)


#define Color_MakeRGB32(__r, __g, __b) \
    ((Color) {kColorType_RGB, .rgb = RGBColor32_Make(__r, __g, __b)})


#define Color_MakeIndex(__idx) \
    ((Color) {kColorType_Index, { .index = __idx}})

#endif /* Color_h */
