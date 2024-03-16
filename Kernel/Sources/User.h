//
//  User.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef User_h
#define User_h

#include <System/Types.h>

enum {
    kRootUserId = 0,
    kRootGroupId = 0
};

typedef struct _User {
    UserId  uid;
    GroupId gid;
} User;


extern User kUser_Root;

#endif /* User_h */
