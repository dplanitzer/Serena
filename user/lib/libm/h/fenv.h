//
//  fenv.h
//  libm
//
//  Created by Dietmar Planitzer on 2/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#if defined(__M68K__) || defined(__M68881) || defined(__M68882)
#include <fenv_m68k.h>
#else
#error "Unsupported platform"
#endif
