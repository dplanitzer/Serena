//
//  Font.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/31/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef Font_h
#define Font_h

#include <stdint.h>


typedef struct Font {
    const char* glyphs;
    int8_t      bbox_width;
    int8_t      bbox_height;
    uint16_t    encoding[128];
} Font;


extern const Font kFont_VT100_US;
extern const Font kFont_VT100_UK;
extern const Font kFont_VT100_LDCS;

#define Font_GetGlyph(__self, __ch) \
((__self)->glyphs + (__self)->encoding[__ch])

#define Font_GetFontWidth(__self) \
(__self)->bbox_width

#define Font_GetFontHeight(__self) \
(__self)->bbox_height

#endif /* Font_h */
