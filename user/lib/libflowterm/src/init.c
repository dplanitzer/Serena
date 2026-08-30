//
//  init.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"


int ft_init(void)
{
    _ft_init_events();

    ft_termin(stdin);
    ft_termout(stdout);

    return 0;
}
