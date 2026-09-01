//
//  flush.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/31/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


void ft_flush(void)
{
    fflush(__ft_termout_fp);
}
