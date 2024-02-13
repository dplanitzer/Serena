//
//  _cmndef.h
//  libsystem
//
//  Created by Dietmar Planitzer on 9/4/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef __ABI_CMNDEF_H
#define __ABI_CMNDEF_H 1


#ifdef __cplusplus
#define __CPP_BEGIN extern "C" {
#else
#define __CPP_BEGIN
#endif

#ifdef __cplusplus
#define __CPP_END }
#else
#define __CPP_END
#endif

#endif /* __ABI_CMNDEF_H */
