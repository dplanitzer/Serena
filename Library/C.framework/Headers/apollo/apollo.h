//
//  apollo.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _APOLLO_H
#define _APOLLO_H 1

#include <_cmndef.h>
#include <apollo/types.h>

__CPP_BEGIN

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2


#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      (O_RDONLY | O_WRONLY)


int open(const char *path, int options);

ssize_t read(int fd, void *buffer, size_t nbytes);
ssize_t write(int fd, const void *buffer, size_t nbytes);

errno_t close(int fd);


pid_t getpid(void);
pid_t getppid(void);

__CPP_END

#endif /* _APOLLO_H */
