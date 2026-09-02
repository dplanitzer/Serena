//
//  save_restore.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 9/1/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

void ft_save(void)
{
    fputs("\0337", __ft_termout_fp);
}

void ft_restore(void)
{
    fputs("\0338", __ft_termout_fp);
}
