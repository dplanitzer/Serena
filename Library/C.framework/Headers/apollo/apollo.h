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
#include <_kbidef.h>
#include <apollo/types.h>

__CPP_BEGIN

typedef struct __process_arguments_t process_arguments_t;
typedef struct __spawn_arguments_t spawn_arguments_t;
typedef struct __waitpid_result_t waitpid_result_t;


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


extern errno_t spawnp(const spawn_arguments_t *args, pid_t *rpid);
extern errno_t waitpid(pid_t pid, waitpid_result_t *result);

extern process_arguments_t *getpargs(void);

__CPP_END

#endif /* _APOLLO_H */
