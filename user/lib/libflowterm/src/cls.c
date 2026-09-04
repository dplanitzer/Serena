//
//  cls.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/3/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_cls(void)
{
    fputs("\033[2J\033[H", __ft_termout_fp);
}
