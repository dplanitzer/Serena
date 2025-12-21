//
//  kpi/_seek.h
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_SEEK_H
#define _KPI_SEEK_H 1

// Specifies how a seek() call should apply 'offset' to the current file
// position.
#define SEEK_SET    0   /* Set the file position to 'offset' */
#define SEEK_CUR    1   /* Add 'offset' to the current file position */
#define SEEK_END    2   /* Add 'offset' to the end of the file */

#endif /* _KPI_SEEK_H */
