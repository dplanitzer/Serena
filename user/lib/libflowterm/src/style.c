//
//  style.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

const ft_color_t ft_ansi_black = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_BLACK}};
const ft_color_t ft_ansi_red = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_RED}};
const ft_color_t ft_ansi_green = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_GREEN}};
const ft_color_t ft_ansi_yellow = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_YELLOW}};
const ft_color_t ft_ansi_blue = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_BLUE}};
const ft_color_t ft_ansi_magenta = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_MAGENTA}};
const ft_color_t ft_ansi_cyan = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_CYAN}};
const ft_color_t ft_ansi_white = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_WHITE}};
const ft_color_t ft_ansi_default = {FT_COLOR_MODEL_ANSI, {.ansi = FT_ANSI_DEFAULT}};


struct style_entry {
    ft_textstyle_t  mask;
    char            ch;
};

static char* _Nonnull __add_textstyle(ft_textstyle_t style, char* _Nonnull p)
{
    static const struct style_entry g_add_entry[] = {
        {FT_BOLD, '1'},
        {FT_DIM, '2'},
        {FT_ITALIC, '3'},
        {FT_UNDERLINE, '4'},
        {FT_BLINK, '5'},
        {FT_INVERSE, '7'},
        {FT_HIDDEN, '8'},
        {FT_STRIKETHROUGH, '9'},
        {0, '\0'},
    };

    const struct style_entry* s = g_add_entry;
    while (s->mask) {
        if ((style & s->mask) != 0) {
            if (*(p - 1) != '[') {
                *p++ = ';';
            }
            *p++ = s->ch;
        }
        s++;
    }

    return p;
}

static char* _Nonnull __reset_textstyle(ft_textstyle_t style, char* _Nonnull p)
{
    static const struct style_entry g_reset_entry[] = {
        {FT_RESET_BOLD_DIM, '2'},
        {FT_RESET_ITALIC, '3'},
        {FT_RESET_UNDERLINE, '4'},
        {FT_RESET_BLINK, '5'},
        {FT_RESET_INVERSE, '7'},
        {FT_RESET_HIDDEN, '8'},
        {FT_RESET_STRIKETHROUGH, '9'},
        {0, '\0'},
    };

    const struct style_entry* s = g_reset_entry;
    while (s->mask) {
        if ((style & s->mask) != 0) {
            if (*(p - 1) != '[') {
                *p++ = ';';
            }
            *p++ = '2';
            *p++ = s->ch;
        }
        s++;
    }

    return p;
}

static char* _Nonnull __add_color(const ft_color_t* _Nonnull clr, bool isFg, char* _Nonnull p)
{
    if (clr->model == FT_COLOR_MODEL_ANSI && clr->color.ansi >= FT_ANSI_BLACK && clr->color.ansi <= FT_ANSI_DEFAULT) {
        if (*(p - 1) != '[') {
            *p++ = ';';
        }
        *p++ = (isFg) ? '3' : '4';
        *p++ = clr->color.ansi + '0';
    }

    return p;
}


void ft_style(ft_textstyle_t style, const ft_color_t* _Nullable fg, const ft_color_t* _Nullable bg)
{
    if (!__ft_termout_do_esc) {
        return;
    }

    
    char* p = __ft_outbuf;

    *p++ = '\033';
    *p++ = '[';

    if ((style & FT_RESET) ==  FT_RESET) {
        *p++ = '0';
    }

    if (style != 0) {
        p = __add_textstyle(style, p);
        p = __reset_textstyle(style, p);
    }

    if (fg) {
        p = __add_color(fg, true, p);
    }

    if (bg) {
        p = __add_color(fg, false, p);
    }

    *p++ = 'm';
    *p = '\0';

    fputs(__ft_outbuf, termout);
}
