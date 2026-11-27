//
//  dd_dir.h
//  diskimage
//
//  Created by Dietmar Planitzer on 11/26/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef DD_DIR_H
#define DD_DIR_H 1

#include <kpi/directory.h>
#include <kpi/types.h>
#include <dd/dd_types.h>

typedef struct dd_dir *dd_dir_t;
typedef unsigned int   dd_ino_t;


// Opens the directory at the filesystem location 'path' for reading. Call
// dd_closedir() once you are done with the directory.
// @Concurrency: Not Safe
extern dd_dir_t dd_opendir(dd_fs_t fs, const char* path);

// Closes the given directory descriptor.
// @Concurrency: Not Safe
extern int dd_closedir(dd_dir_t dir);

// Reads the next directory entry and returns a pointer to it if it exists.
// Returns NULL if no more entry exists in the directory.
// Call dd_rewinddir() to rewind to the beginning of the directory listing and to
// start over again with the reading process.
// @Concurrency: Not Safe
extern const dir_entry_t* dd_dirnext(dd_dir_t dir);

// Resets the read position of the directory identified by 'dir' to the beginning.
// The next dd_dirnext() call will start reading directory entries from the
// beginning of the directory.
// @Concurrency: Not Safe
extern void dd_rewinddir(dd_dir_t dir);

#endif /* DD_DIR_H */
