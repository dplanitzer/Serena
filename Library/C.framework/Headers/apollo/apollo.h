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
#include <_syslimits.h>
#include <apollo/types.h>
#include <time.h>

__CPP_BEGIN

typedef struct __process_arguments_t process_arguments_t;
typedef struct __spawn_arguments_t spawn_arguments_t;
typedef struct __waitpid_result_t waitpid_result_t;


#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2


// XXX make this SEEK_XXX and make stdio.h pick this up (and just this), once we're putting the real API in place
#define S_WHENCE_SET    0
#define S_WHENCE_CUR    1
#define S_WHENCE_END    2


#define PATH_MAX __PATH_MAX
#define NAME_MAX __PATH_COMPONENT_MAX


extern int creat(const char* path, int options, int permissions);
extern int open(const char *path, int options);
extern errno_t opendir(const char* path, int* fd);

extern ssize_t read(int fd, void *buffer, size_t nbytes);
extern ssize_t write(int fd, const void *buffer, size_t nbytes);

extern errno_t tell(int fd, off_t* pos);
extern errno_t seek(int fd, off_t offset, off_t *oldpos, int whence);

extern errno_t close(int fd);


extern errno_t getcwd(char* buffer, size_t bufferSize);
extern errno_t setcwd(const char* path);


extern errno_t getfileinfo(const char* path, struct _file_info_t* info);
extern errno_t setfileinfo(const char* path, struct _mutable_file_info_t* info);

extern errno_t fgetfileinfo(int fd, struct _file_info_t* info);
extern errno_t fsetfileinfo(int fd, struct _mutable_file_info_t* info);


extern errno_t truncate(const char *path, off_t length);
extern errno_t ftruncate(int fd, off_t length);


extern errno_t ioctl(int fd, int cmd, ...);
extern errno_t fdctl(int fd, int cmd, ...);


extern errno_t access(const char* path, int mode);
extern errno_t unlink(const char* path);    // deletes files and (empty) directories
extern errno_t rename(const char* oldpath, const char* newpath);
extern errno_t mkdir(const char* path, mode_t mode);


extern mode_t getumask(void);
extern void setumask(mode_t mask);


extern pid_t getpid(void);
extern pid_t getppid(void);


extern uid_t getuid(void);


extern errno_t spawnp(const spawn_arguments_t *args, pid_t *rpid);
extern errno_t waitpid(pid_t pid, waitpid_result_t *result);

extern process_arguments_t *getpargs(void);


extern errno_t nanosleep(const struct timespec *delay);
extern errno_t usleep(useconds_t delay);
extern errno_t sleep(time_t delay);

__CPP_END

#endif /* _APOLLO_H */
