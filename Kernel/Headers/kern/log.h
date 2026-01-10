//
//  log.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _LOG_H
#define _LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <kpi/types.h>


// Initialize the log package. It's save to call this before kalloc is initialized.
// Configures the logging system such that all log messages are written to the log
// ring buffer.
extern void log_init(void);

// Switches the log package from the ring buffer to the kernel console. Note that
// once switched to the kernel console, there's no way to switch back to the ring
// buffer. This function should only be called when the machine encountered a
// fatal error, so that we can print a message to the screen. Returns true if
// output is connected to the kernel console; false otherwise.
extern bool log_switch_to_console(void);

// Writes the given byte sequence to the log sink
extern void log_write(const char* _Nonnull buf, ssize_t nbytes);

// Reads 'nbytes' from the log buffer and writes the bytes to 'buf'. Returns the
// number of bytes actually read. This number will be 0 if the log buffer is
// empty or the log is connected to the kernel console.
extern ssize_t log_read(void* _Nonnull buf, ssize_t nbytes);

// Returns a pointer to the log buffer.
extern const char* _Nonnull log_buffer(void);


extern void printf(const char* _Nonnull format, ...);
extern void vprintf(const char* _Nonnull format, va_list ap);

#endif /* _LOG_H */
