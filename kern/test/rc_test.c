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
    assert_int_eq(2, rc);

    rc_retain(&rc);
    assert_int_eq(3, rc);


    assert_bool_eq(false, rc_release(&rc));
    assert_int_eq(2, rc);

    assert_bool_eq(false, rc_release(&rc));
    assert_int_eq(1, rc);

    assert_bool_eq(true, rc_release(&rc));
    assert_int_eq(0, rc);

    // A real app should never do this because this means that it's trying to
    // release a reference it doesn't own. However we still want to make sure
    // that such a scenario doesn't trigger a spurious duplicate deallocation
    // of already deallocated data.
    assert_bool_eq(false, rc_release(&rc));
    assert_int_eq(-1, rc);
}
