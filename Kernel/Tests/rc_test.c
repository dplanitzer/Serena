//
//  rc_tests.c
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/5/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <ext/rc.h>
#include "Asserts.h"


void rc_test(int argc, char *argv[])
{
    ref_count_t rc = RC_INIT;

    rc_retain(&rc);
    assertEquals(2, rc);

    rc_retain(&rc);
    assertEquals(3, rc);


    assertEquals(false, rc_release(&rc));
    assertEquals(2, rc);

    assertEquals(false, rc_release(&rc));
    assertEquals(1, rc);

    assertEquals(true, rc_release(&rc));
    assertEquals(0, rc);

    // A real app should never do this because this means that it's trying to
    // release a reference it doesn't own. However we still want to make sure
    // that such a scenario doesn't trigger a spurious duplicate deallocation
    // of already deallocated data.
    assertEquals(false, rc_release(&rc));
    assertEquals(-1, rc);
}
