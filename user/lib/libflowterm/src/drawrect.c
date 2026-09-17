//
//  drawrect.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/16/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '-';
const ft_rectstyle ft_rectstyle_hflat = {'-', '-', '-', '|', '-', '-', '-', '|'};

// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '|';
const ft_rectstyle ft_rectstyle_vflat = {'|', '-', '|', '|', '|', '-', '|', '|'};

// Draw top and bottom edges with '-', left and right edges with '|' and all
// corners with '+';
const ft_rectstyle ft_rectstyle_plus = {'+', '-', '+', '|', '+', '-', '+', '|'};



void ft_drawrect(const ft_rectstyle* _Nonnull style, int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }


    // escape sequence to move the cursor back 'width' characters and down one line
    char* r_to_l = __ft_outbuf_alt;
    const char* r_to_l0 = r_to_l;
    *r_to_l++ = '\033';
    *r_to_l++ = '[';
    *r_to_l++ = 'B';
    *r_to_l++ = '\033';
    *r_to_l++ = '[';
    *r_to_l++ = 'D';
    *r_to_l++ = style->right;
    *r_to_l++ = '\033';
    *r_to_l++ = '[';
     r_to_l = __ft_itoa(width, r_to_l);
    *r_to_l++ = 'D';
    *r_to_l++ = style->left;
    *r_to_l++ = '\0';

    char* l_to_r = r_to_l;
    const char* l_to_r0 = l_to_r;
    *l_to_r++ = '\033';
    *l_to_r++ = '[';
    *l_to_r++ = 'B';
    *l_to_r++ = '\033';
    *l_to_r++ = '[';
    *l_to_r++ = 'D';
    *l_to_r++ = style->left;
    *l_to_r++ = '\033';
    *l_to_r++ = '[';
     l_to_r = __ft_itoa(width - 2, l_to_r);
    *l_to_r++ = 'C';
    *l_to_r++ = style->right;
    *l_to_r   = '\0';


    putc(style->top_left, __ft_termout_fp);
    ft_hline(style->top, width - 2);
    putc(style->top_right, __ft_termout_fp);

    for (int y = 0; y < height - 2; y++) {
        const char* p = (y & 1) ? l_to_r0 : r_to_l0;

        fputs(p, __ft_termout_fp);
    }

    if ((height - 3) & 1) {
        char* p = __ft_outbuf_alt;

        *p++ = '\033';
        *p++ = '[';
        *p++ = 'B';
        *p++ = '\033';
        *p++ = '[';
         p = __ft_itoa(width, p);
        *p++ = 'D';
        *p++ = '\0';
    }
    else {
        char* p = __ft_outbuf_alt;

        *p++ = '\033';
        *p++ = '[';
        *p++ = 'B';
        *p++ = '\033';
        *p++ = '[';
        *p++ = 'D';
        *p = '\0';
    }
    fputs(__ft_outbuf_alt, __ft_termout_fp);

    putc(style->bottom_left, __ft_termout_fp);
    ft_hline(style->bottom, width - 2);
    putc(style->bottom_right, __ft_termout_fp);
}
