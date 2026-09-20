//
//  init.c
//  libflowterm
//
//  Created by Dietmar Planitzer on 8/22/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "__flowterm.h"

char __ft_outbuf[_FT_OUTBUF_SIZE];
char __ft_outbuf_alt[_FT_OUTBUF_SIZE];


int ft_init(unsigned int flags)
{
    __ft_init_events();

    ft_termin(stdin);
    ft_termout(stdout);
    
    ft_insertmode(FT_OFF);
    
    return 0;
}
