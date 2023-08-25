//
//  Foundation.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Foundation_h
#define Foundation_h

#include <Runtime.h>
#include <assert.h>


#define SIZE_GB(x)  ((Int)(x) * 1024 * 1024 * 1024)
#define SIZE_MB(x)  ((Int)(x) * 1024 * 1024)
#define SIZE_KB(x)  ((Int)(x) * 1024)


// Macros to detect errors and to jump to the 'failed:' label if an error is detected.

// Declares an error variable 'err' which is assigned to by the try_xxx macros
// and available at the 'catch' label
#define decl_try_err()      ErrorCode err = EOK

// Go to the 'catch' label if 'f' does not return EOK. The error returned by 'f'
// is assigned to 'err'. Call this instead of 'try_bang' if you are calling a
// failable function and it is by design expected that the function can actually
// fail at runtime and there is a way to recover from the failure. Most of the
// time 'recovering' from a failure will simply mean to abort the system call
// and to return to user space. The application will then need to figure out
// how to proceed.
#define try(f)              if ((err = (f)) != EOK) goto catch

// Go to the 'catch' label if 'f' returns a NULL pointer. The pointer is stored
// in the variable 'p'. 'e' is the error that should be assigned to 'err'
#define try_null(p, f, e)   if ((p = (f)) == NULL) { err = e; goto catch; }

// Halt the machine if the function 'f' does not return EOK. Use this instead of
// 'try' if you are calling a failable function but based on the design of the
// code the function you call should never fail in actual reality.
#define try_bang(f)         if ((f) != EOK) { abort(); }

// Set 'err' to the given error and go to the 'catch' label if the given pointer
// is null. Otherwise fall through to the next statement.
#define throw_ifnull(p, e)  if ((p) == NULL) { err = e; goto catch; }

// Set 'err' to the given error and go to the 'catch' label.
#define throw(e)            err = e; goto catch;


// Error code definitions (keep in sync with lowmem.i)
typedef Int ErrorCode;
#define EOK         0
#define ENOMEM      1
#define ENODATA     2
#define ENOTCDF     3
#define ENOBOOT     4
#define ENODRIVE    5
#define EDISKCHANGE 6
#define ETIMEDOUT   7
#define ENODEV      8
#define EPARAM      9
#define ERANGE      10
#define EINTR       11
#define EAGAIN      12
#define EPIPE       13
#define EBUSY       14
#define ENOSYS      15
#define EINVAL      16
#define EIO         17
#define EPERM       18
#define EDEADLK     19
#define EDOM        20
#define EILSEQ      21


// Variable argument lists
typedef unsigned char *va_list;

#define __va_align(type) (__alignof(type)>=4?__alignof(type):4)
#define __va_do_align(vl,type) ((vl)=(unsigned char *)((((unsigned int)(vl))+__va_align(type)-1)/__va_align(type)*__va_align(type)))
#define __va_mem(vl,type) (__va_do_align((vl),type),(vl)+=sizeof(type),((type*)(vl))[-1])

#define va_start(ap, lastarg) ((ap)=(va_list)(&lastarg+1))
#define va_arg(vl,type) __va_mem(vl,type)
#define va_end(vl) ((vl)=0)
#define va_copy(new,old) ((new)=(old))


// A callback function that takes a single (context) pointer argument
typedef void (* _Nonnull Closure1Arg_Func)(Byte* _Nullable pContext);


// Int64
extern const Character* _Nonnull Int64_ToString(Int64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// UInt64
extern const Character* _Nonnull UInt64_ToString(UInt64 val, Int base, Int fieldWidth, Character paddingChar, Character* _Nonnull pString, Int maxLength);


// String
extern Bool String_Equals(const Character* _Nonnull pLhs, const Character* _Nonnull pRhs);


// TimeInterval

// Represents time as measured in seconds and nanoseconds. All TimeInterval functions
// expect time interval inputs in canonical form - meaning the nanoseconds field
// is in the range [0..1000_000_000). Negative time interval values are represented
// with a negative seconds field if seconds != 0 and a negative nanoseconds field if
// seconds == 0 and nanoseconds != 0.
// The TimeInterval type is a saturating type. This means that a time value is set to
// +/-infinity if a computation overflows / underflows.
typedef struct _TimeInterval {
    Int32   seconds;
    Int32   nanoseconds;        // 0..<1billion
} TimeInterval;


inline TimeInterval TimeInterval_Make(Int32 seconds, Int32 nanoseconds) {
    TimeInterval ti;
    ti.seconds = seconds;
    ti.nanoseconds = nanoseconds;
    return ti;
}

inline TimeInterval TimeInterval_MakeSeconds(Int32 seconds) {
    TimeInterval ti;
    ti.seconds = seconds;
    ti.nanoseconds = 0;
    return ti;
}

inline TimeInterval TimeInterval_MakeMilliseconds(Int32 millis) {
    TimeInterval ti;
    ti.seconds = millis / 1000;
    ti.nanoseconds = (millis - (ti.seconds * 1000)) * 1000 * 1000;
    return ti;
}

inline TimeInterval TimeInterval_MakeMicroseconds(Int32 micros) {
    TimeInterval ti;
    ti.seconds = micros / (1000 * 1000);
    ti.nanoseconds = (micros - (ti.seconds * 1000 * 1000)) * 1000;
    return ti;
}

inline Bool TimeInterval_IsNegative(TimeInterval ti) {
    return ti.seconds < 0 || ti.nanoseconds < 0;
}

inline Bool TimeInterval_Equals(TimeInterval t0, TimeInterval t1) {
    return t0.nanoseconds == t1.nanoseconds && t0.seconds == t1.seconds;
}

inline Bool TimeInterval_Less(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds < t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds < t1.nanoseconds));
}

inline Bool TimeInterval_LessEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds < t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds <= t1.nanoseconds));
}

inline Bool TimeInterval_Greater(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds > t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds > t1.nanoseconds));
}

inline Bool TimeInterval_GreaterEquals(TimeInterval t0, TimeInterval t1) {
    return (t0.seconds > t1.seconds || (t0.seconds == t1.seconds && t0.nanoseconds >= t1.nanoseconds));
}

extern TimeInterval TimeInterval_Add(TimeInterval t0, TimeInterval t1);

extern TimeInterval TimeInterval_Subtract(TimeInterval t0, TimeInterval t1);


#define kQuantums_Infinity      INT32_MAX
#define kQuantums_MinusInfinity INT32_MIN
extern const TimeInterval       kTimeInterval_Zero;
extern const TimeInterval       kTimeInterval_Infinity;
extern const TimeInterval       kTimeInterval_MinusInfinity;


// Printing
extern void print_init(void);
extern void print(const Character* _Nonnull format, ...);
extern void printv(const Character* _Nonnull format, va_list ap);

typedef void (* _Nonnull PrintSink_Func)(void* _Nullable pContext, const Character* _Nonnull pString);

extern void _printv(PrintSink_Func _Nonnull pSinkFunc, void* _Nullable pContext, Character* _Nonnull pBuffer, Int bufferCapacity, const Character* _Nonnull format, va_list ap);

#endif /* Foundation_h */
