//
//  sys/_unistd.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_PRIV_UNISTD_H
#define _SYS_PRIV_UNISTD_H 1

#define R_OK    1
#define W_OK    2
#define X_OK    4
#define F_OK    0

// Specifies how a seek() call should apply 'offset' to the current file
// position.
#define SEEK_SET    0   /* Set the file position to 'offset' */
#define SEEK_CUR    1   /* Add 'offset' to the current file position */
#define SEEK_END    2   /* Add 'offset' to the end of the file */

#endif /* _SYS_PRIV_UNISTD_H */
