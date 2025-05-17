//
//  Color.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/24/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Color_h
#define Color_h

#include <kpi/fb.h>


typedef enum ColorType {
    kColorType_RGB32,
    kColorType_Index
} ColorType;


// A color type
typedef struct Color {
    ColorType   tag;
    union {
        RGBColor32  rgb32;
        int         index;
    }           u;
} Color;


#define Color_MakeRGB32(__r, __g, __b) \
    ((Color) {kColorType_RGB, .rgb = RGBColor32_Make(__r, __g, __b)})


#define Color_MakeIndex(__idx) \
    ((Color) {kColorType_Index, { .index = __idx}})

#endif /* Color_h */
