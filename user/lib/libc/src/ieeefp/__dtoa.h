/****************************************************************
 *
 * The author of this software is David M. Gay.
 *
 * Copyright (c) 1991, 2000, 2001 by Lucent Technologies.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose without fee is hereby granted, provided that this entire notice
 * is included in all copies of any software which is or includes a copy
 * or modification of this software and in all copies of the supporting
 * documentation for such software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTY.  IN PARTICULAR, NEITHER THE AUTHOR NOR LUCENT MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE MERCHANTABILITY
 * OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 ***************************************************************/

/* Please send bug reports to David M. Gay (dmg at acm dot org,
 * with " at " changed at "@" and " dot " changed to ".").	*/

 /* On a machine with IEEE extended-precision registers, it is
 * necessary to specify double-precision (53-bit) rounding precision
 * before invoking strtod or dtoa.  If the machine uses (the equivalent
 * of) Intel 80x87 arithmetic, the call
 *	_control87(PC_53, MCW_PC);
 * does this with many compilers.  Whether this or another call is
 * appropriate depends on the compiler; for this to work, it may be
 * necessary to #include "float.h" or another system-dependent header
 * file.  When needed, this call avoids double rounding, which can
 * cause one bit errors, e.g., with strtod on 8.3e26 or 6.3876e-16.
 */

/* strtod for IEEE-, VAX-, and IBM-arithmetic machines.
 * (Note that IEEE arithmetic is disabled by gcc's -ffast-math flag.)
 *
 * This strtod returns a nearest machine number to the input decimal
 * string (or sets errno to ERANGE).  With IEEE arithmetic, ties are
 * broken by the IEEE round-even rule.  Otherwise ties are broken by
 * biased rounding (add half and chop).
 *
 * Inspired loosely by William D. Clinger's paper "How to Read Floating
 * Point Numbers Accurately" [Proc. ACM SIGPLAN '90, pp. 92-101].
 *
 * Modifications:
 *
 *	1. We only require IEEE, IBM, or VAX double-precision
 *		arithmetic (not IEEE double-extended).
 *	2. We get by with floating-point arithmetic in a case that
 *		Clinger missed -- when we're computing d * 10^n
 *		for a small integer d and the integer n is not too
 *		much larger than 22 (the maximum integer k for which
 *		we can represent 10^k exactly), we may be able to
 *		compute (d*10^k) * 10^(e-k) with just one roundoff.
 *	3. Rather than a bit-at-a-time adjustment of the binary
 *		result in the hard case, we use floating-point
 *		arithmetic to determine the adjustment to within
 *		one bit; only in really hard cases do we need to
 *		compute a second residual.
 *	4. Because of 3., we don't need a large table of powers of 10
 *		for ten-to-e (just some small tables, e.g. of 10^k
 *		for 0 <= k <= 22).
 */

/*
 * #define IEEE_8087 for IEEE-arithmetic machines where the least
 *	significant byte has the lowest address.
 * #define IEEE_MC68k for IEEE-arithmetic machines where the most
 *	significant byte has the lowest address.
 * #define Long int on machines with 32-bit ints and 64-bit longs.
 * #define IBM for IBM mainframe-style floating-point arithmetic.
 * #define VAX for VAX-style floating-point arithmetic (D_floating).
 * #define No_leftright to omit left-right logic in fast floating-point
 *	computation of dtoa.  This will cause dtoa modes 4 and 5 to be
 *	treated the same as modes 2 and 3 for some inputs.
 * #define Honor_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *	and strtod and dtoa should round accordingly.  Unless Trust_FLT_ROUNDS
 *	is also #defined, fegetround() will be queried for the rounding mode.
 *	Note that both FLT_ROUNDS and fegetround() are specified by the C99
 *	standard (and are specified to be consistent, with fesetround()
 *	affecting the value of FLT_ROUNDS), but that some (Linux) systems
 *	do not work correctly in this regard, so using fegetround() is more
 *	portable than using FLT_ROUNDS directly.
 * #define Check_FLT_ROUNDS if FLT_ROUNDS can assume the values 2 or 3
 *	and Honor_FLT_ROUNDS is not #defined.
 * #define RND_PRODQUOT to use rnd_prod and rnd_quot (assembly routines
 *	that use extended-precision instructions to compute rounded
 *	products and quotients) with IBM.
 * #define ROUND_BIASED for IEEE-format with biased rounding and arithmetic
 *	that rounds toward +Infinity.
 * #define ROUND_BIASED_without_Round_Up for IEEE-format with biased
 *	rounding when the underlying floating-point arithmetic uses
 *	unbiased rounding.  This prevent using ordinary floating-point
 *	arithmetic when the result could be computed with one rounding error.
 * #define Inaccurate_Divide for IEEE-format with correctly rounded
 *	products but inaccurate quotients, e.g., for Intel i860.
 * #define NO_LONG_LONG on machines that do not have a "long long"
 *	integer type (of >= 64 bits).  On such machines, you can
 *	#define Just_16 to store 16 bits per 32-bit Long when doing
 *	high-precision integer arithmetic.  Whether this speeds things
 *	up or slows things down depends on the machine and the number
 *	being converted.  If long long is available and the name is
 *	something other than "long long", #define Llong to be the name,
 *	and if "unsigned Llong" does not work as an unsigned version of
 *	Llong, #define #ULLong to be the corresponding unsigned type.
 * #define Bad_float_h if your system lacks a float.h or if it does not
 *	define some or all of DBL_DIG, DBL_MAX_10_EXP, DBL_MAX_EXP,
 *	FLT_RADIX, FLT_ROUNDS, and DBL_MAX.
 * #define MALLOC your_malloc, where your_malloc(n) acts like malloc(n)
 *	if memory is available and otherwise does something you deem
 *	appropriate.  If MALLOC is undefined, malloc will be invoked
 *	directly -- and assumed always to succeed.  Similarly, if you
 *	want something other than the system's free() to be called to
 *	recycle memory acquired from MALLOC, #define FREE to be the
 *	name of the alternate routine.  (FREE or free is only called in
 *	pathological cases, e.g., in a dtoa call after a dtoa return in
 *	mode 3 with thousands of digits requested.)
 * #define Omit_Private_Memory to omit logic (added Jan. 1998) for making
 *	memory allocations from a private pool of memory when possible.
 *	When used, the private pool is PRIVATE_MEM bytes long:  2304 bytes,
 *	unless #defined to be a different length.  This default length
 *	suffices to get rid of MALLOC calls except for unusual cases,
 *	such as decimal-to-binary conversion of a very long string of
 *	digits.  The longest string dtoa can return is about 751 bytes
 *	long.  For conversions by strtod of strings of 800 digits and
 *	all dtoa conversions in single-threaded executions with 8-byte
 *	pointers, PRIVATE_MEM >= 7400 appears to suffice; with 4-byte
 *	pointers, PRIVATE_MEM >= 7112 appears adequate.
 * #define NO_INFNAN_CHECK if you do not wish to have INFNAN_CHECK
 *	#defined automatically on IEEE systems.  On such systems,
 *	when INFNAN_CHECK is #defined, strtod checks
 *	for Infinity and NaN (case insensitively).  On some systems
 *	(e.g., some HP systems), it may be necessary to #define NAN_WORD0
 *	appropriately -- to the most significant word of a quiet NaN.
 *	(On HP Series 700/800 machines, -DNAN_WORD0=0x7ff40000 works.)
 *	When INFNAN_CHECK is #defined and No_Hex_NaN is not #defined,
 *	strtod also accepts (case insensitively) strings of the form
 *	NaN(x), where x is a string of hexadecimal digits and spaces;
 *	if there is only one string of hexadecimal digits, it is taken
 *	for the 52 fraction bits of the resulting NaN; if there are two
 *	or more strings of hex digits, the first is for the high 20 bits,
 *	the second and subsequent for the low 32 bits, with intervening
 *	white space ignored; but if this results in none of the 52
 *	fraction bits being on (an IEEE Infinity symbol), then NAN_WORD0
 *	and NAN_WORD1 are used instead.
 * #define MULTIPLE_THREADS if the system offers preemptively scheduled
 *	multiple threads.  In this case, you must provide (or suitably
 *	#define) two locks, acquired by ACQUIRE_DTOA_LOCK(n) and freed
 *	by FREE_DTOA_LOCK(n) for n = 0 or 1.  (The second lock, accessed
 *	in pow5mult, ensures lazy evaluation of only one copy of high
 *	powers of 5; omitting this lock would introduce a small
 *	probability of wasting memory, but would otherwise be harmless.)
 *	You must also invoke freedtoa(s) to free the value s returned by
 *	dtoa.  You may do so whether or not MULTIPLE_THREADS is #defined.

 *	When MULTIPLE_THREADS is #defined, this source file provides
 *		void set_max_dtoa_threads(unsigned int n);
 *	and expects
 *		unsigned int dtoa_get_threadno(void);
 *	to be available (possibly provided by
 *		#define dtoa_get_threadno omp_get_thread_num
 *	if OpenMP is in use or by
 *		#define dtoa_get_threadno pthread_self
 *	if Pthreads is in use), to return the current thread number.
 *	If set_max_dtoa_threads(n) was called and the current thread
 *	number is k with k < n, then calls on ACQUIRE_DTOA_LOCK(...) and
 *	FREE_DTOA_LOCK(...) are avoided; instead each thread with thread
 *	number < n has a separate copy of relevant data structures.
 *	After set_max_dtoa_threads(n), a call set_max_dtoa_threads(m)
 *	with m <= n has has no effect, but a call with m > n is honored.
 *	Such a call invokes REALLOC (assumed to be "realloc" if REALLOC
 *	is not #defined) to extend the size of the relevant array.

 * #define NO_IEEE_Scale to disable new (Feb. 1997) logic in strtod that
 *	avoids underflows on inputs whose result does not underflow.
 *	If you #define NO_IEEE_Scale on a machine that uses IEEE-format
 *	floating-point numbers and flushes underflows to zero rather
 *	than implementing gradual underflow, then you must also #define
 *	Sudden_Underflow.
 * #define USE_LOCALE to use the current locale's decimal_point value.
 * #define SET_INEXACT if IEEE arithmetic is being used and extra
 *	computation should be done to set the inexact flag when the
 *	result is inexact and avoid setting inexact when the result
 *	is exact.  In this case, dtoa.c must be compiled in
 *	an environment, perhaps provided by #include "dtoa.c" in a
 *	suitable wrapper, that defines two functions,
 *		int get_inexact(void);
 *		void clear_inexact(void);
 *	such that get_inexact() returns a nonzero value if the
 *	inexact bit is already set, and clear_inexact() sets the
 *	inexact bit to 0.  When SET_INEXACT is #defined, strtod
 *	also does extra computations to set the underflow and overflow
 *	flags when appropriate (i.e., when the result is tiny and
 *	inexact or when it is a numeric value rounded to +-infinity).
 * #define NO_ERRNO if strtod should not assign errno = ERANGE when
 *	the result overflows to +-Infinity or underflows to 0.
 *	When errno should be assigned, under seemingly rare conditions
 *	it may be necessary to define Set_errno(x) suitably, e.g., in
 *	a local errno.h, such as
 *		#include <errno.h>
 *		#define Set_errno(x) _set_errno(x)
 * #define NO_HEX_FP to omit recognition of hexadecimal floating-point
 *	values by strtod.
 * #define NO_STRTOD_BIGCOMP (on IEEE-arithmetic systems only for now)
 *	to disable logic for "fast" testing of very long input strings
 *	to strtod.  This testing proceeds by initially truncating the
 *	input string, then if necessary comparing the whole string with
 *	a decimal expansion to decide close cases. This logic is only
 *	used for input more than STRTOD_DIGLIM digits long (default 40).
 */

#ifndef __DTOA_H
#define __DTOA_H 1

#include "dtoa.h"

#include <sys/mtx.h>
#include <sys/vcpu.h>

#ifdef __IEEE_LITTLE_ENDIAN
#define IEEE_8087
#endif

#ifdef __IEEE_BIG_ENDIAN
#define IEEE_MC68k
#endif


extern mtx_t __g_dtoa_mtx[2];

#if defined(MULTIPLE_THREADS)
#define dtoa_get_threadno() ((unsigned int)vcpu_id(vcpu_self()))
#define ACQUIRE_DTOA_LOCK(x) mtx_lock(&__g_dtoa_mtx[x])
#define FREE_DTOA_LOCK(x) mtx_unlock(&__g_dtoa_mtx[x])
#endif

#ifndef Long
#define Long int
#endif
#ifndef ULong
typedef unsigned Long ULong;
#endif

#ifdef DTOA_DEBUG
#include <assert.h>
#include <stdio.h>
#define Bug(x) {fprintf(stderr, "%s\n", x); exit(1);}
#define Debug(x) x
extern int __dtoa_stats[7]; /* strtod_{64,96,bigcomp},dtoa_{exact,64,96,bigcomp} */
#else
#define assert(x) /*nothing*/
#define Debug(x) /*nothing*/
#endif

#include <stdlib.h>
#include <string.h>

#ifdef USE_LOCALE
#include "locale.h"
#endif

#ifdef Honor_FLT_ROUNDS
#ifndef Trust_FLT_ROUNDS
#include <fenv.h>
#endif
#endif

__CPP_BEGIN
#ifdef MALLOC
extern void *MALLOC(size_t);
#else
#define MALLOC malloc
#endif

#ifdef REALLOC
extern void *REALLOC(void*,size_t);
#else
#define REALLOC realloc
#endif

#ifndef FREE
#define FREE free
#endif
__CPP_END

#undef IEEE_Arith
#undef Avoid_Underflow
#ifdef IEEE_MC68k
#define IEEE_Arith
#endif
#ifdef IEEE_8087
#define IEEE_Arith
#endif

#ifdef IEEE_Arith
#ifndef NO_INFNAN_CHECK
#undef INFNAN_CHECK
#define INFNAN_CHECK
#endif
#else
#undef INFNAN_CHECK
#define NO_STRTOD_BIGCOMP
#endif

#include <errno.h>

#ifdef NO_ERRNO /*{*/
#undef Set_errno
#define Set_errno(x)
#else
#ifndef Set_errno
#define Set_errno(x) errno = x
#endif
#endif /*}*/

#ifdef Bad_float_h

#ifdef IEEE_Arith
#define DBL_DIG 15
#define DBL_MAX_10_EXP 308
#define DBL_MAX_EXP 1024
#define FLT_RADIX 2
#endif /*IEEE_Arith*/

#ifdef IBM
#define DBL_DIG 16
#define DBL_MAX_10_EXP 75
#define DBL_MAX_EXP 63
#define FLT_RADIX 16
#define DBL_MAX 7.2370055773322621e+75
#endif

#ifdef VAX
#define DBL_DIG 16
#define DBL_MAX_10_EXP 38
#define DBL_MAX_EXP 127
#define FLT_RADIX 2
#define DBL_MAX 1.7014118346046923e+38
#endif

#ifndef LONG_MAX
#define LONG_MAX 2147483647
#endif

#else /* ifndef Bad_float_h */
#include <float.h>
#endif /* Bad_float_h */

#ifndef __MATH_H__
#include <math.h>
#endif

#ifndef Omit_Private_Memory
#ifndef PRIVATE_MEM
#define PRIVATE_MEM 2304
#endif
#define PRIVATE_mem ((PRIVATE_MEM+sizeof(double)-1)/sizeof(double))
#endif

#if defined(IEEE_8087) + defined(IEEE_MC68k) + defined(VAX) + defined(IBM) != 1
Exactly one of IEEE_8087, IEEE_MC68k, VAX, or IBM should be defined.
#endif

#undef USE_BF96

#ifdef NO_LONG_LONG /*{{*/
#undef ULLong
#ifdef Just_16
#undef Pack_32
/* When Pack_32 is not defined, we store 16 bits per 32-bit Long.
 * This makes some inner loops simpler and sometimes saves work
 * during multiplications, but it often seems to make things slightly
 * slower.  Hence the default is now to store 32 bits per Long.
 */
#endif
#else	/*}{ long long available */
#ifndef Llong
#define Llong long long
#endif
#ifndef ULLong
#define ULLong unsigned Llong
#endif
#ifndef NO_BF96 /*{*/
#define USE_BF96

#ifdef SET_INEXACT
#define dtoa_divmax 27
#endif

typedef struct BF96 {		/* Normalized 96-bit software floating point numbers */
	unsigned int b0,b1,b2;	/* b0 = most significant, binary point just to its left */
	int e;			/* number represented = b * 2^e, with .5 <= b < 1 */
	} BF96;
#endif /*}*/
#endif /*}} NO_LONG_LONG */

typedef union { double d; ULong L[2];
#ifdef USE_BF96
	ULLong LL;
#endif
	} U;

#ifdef IEEE_8087
#define word0(x) (x)->L[1]
#define word1(x) (x)->L[0]
#else
#define word0(x) (x)->L[0]
#define word1(x) (x)->L[1]
#endif
#define dval(x) (x)->d
#define LLval(x) (x)->LL

#ifndef STRTOD_DIGLIM
#define STRTOD_DIGLIM 40
#endif

#ifdef DIGLIM_DEBUG
extern int __strtod_diglim;
#else
#define __strtod_diglim STRTOD_DIGLIM
#endif

/* The following definition of Storeinc is appropriate for MIPS processors.
 * An alternative that might be better on some machines is
 * #define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
 */
#if defined(IEEE_8087) + defined(VAX)
#define Storeinc(a,b,c) (((unsigned short *)a)[1] = (unsigned short)b, \
((unsigned short *)a)[0] = (unsigned short)c, a++)
#else
#define Storeinc(a,b,c) (((unsigned short *)a)[0] = (unsigned short)b, \
((unsigned short *)a)[1] = (unsigned short)c, a++)
#endif

/* #define P DBL_MANT_DIG */
/* Ten_pmax = floor(P*log(2)/log(5)) */
/* Bletch = (highest power of 2 < DBL_MAX_10_EXP) / 16 */
/* Quick_max = floor((P-1)*log(FLT_RADIX)/log(10) - 1) */
/* Int_max = floor(P*log(FLT_RADIX)/log(10) - 1) */

#ifdef IEEE_Arith
#define Exp_shift  20
#define Exp_shift1 20
#define Exp_msk1    0x100000
#define Exp_msk11   0x100000
#define Exp_mask  0x7ff00000
#define P 53
#define Nbits 53
#define Bias 1023
#define Emax 1023
#define Emin (-1022)
#define Exp_1  0x3ff00000
#define Exp_11 0x3ff00000
#define Ebits 11
#define Frac_mask  0xfffff
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Bletch 0x10
#define Bndry_mask  0xfffff
#define Bndry_mask1 0xfffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 1
#define Tiny0 0
#define Tiny1 1
#define Quick_max 14
#define Int_max 14
#ifndef NO_IEEE_Scale
#define Avoid_Underflow
#ifdef Flush_Denorm	/* debugging option */
#undef Sudden_Underflow
#endif
#endif

#ifndef Flt_Rounds
#ifdef FLT_ROUNDS
#define Flt_Rounds FLT_ROUNDS
#else
#define Flt_Rounds 1
#endif
#endif /*Flt_Rounds*/

#ifdef Honor_FLT_ROUNDS
#undef Check_FLT_ROUNDS
#define Check_FLT_ROUNDS
#else
#define Rounding Flt_Rounds
#endif

#else /* ifndef IEEE_Arith */
#undef Check_FLT_ROUNDS
#undef Honor_FLT_ROUNDS
#undef SET_INEXACT
#undef  Sudden_Underflow
#define Sudden_Underflow
#ifdef IBM
#undef Flt_Rounds
#define Flt_Rounds 0
#define Exp_shift  24
#define Exp_shift1 24
#define Exp_msk1   0x1000000
#define Exp_msk11  0x1000000
#define Exp_mask  0x7f000000
#define P 14
#define Nbits 56
#define Bias 65
#define Emax 248
#define Emin (-260)
#define Exp_1  0x41000000
#define Exp_11 0x41000000
#define Ebits 8	/* exponent has 7 bits, but 8 is the right value in b2d */
#define Frac_mask  0xffffff
#define Frac_mask1 0xffffff
#define Bletch 4
#define Ten_pmax 22
#define Bndry_mask  0xefffff
#define Bndry_mask1 0xffffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 4
#define Tiny0 0x100000
#define Tiny1 0
#define Quick_max 14
#define Int_max 15
#else /* VAX */
#undef Flt_Rounds
#define Flt_Rounds 1
#define Exp_shift  23
#define Exp_shift1 7
#define Exp_msk1    0x80
#define Exp_msk11   0x800000
#define Exp_mask  0x7f80
#define P 56
#define Nbits 56
#define Bias 129
#define Emax 126
#define Emin (-129)
#define Exp_1  0x40800000
#define Exp_11 0x4080
#define Ebits 8
#define Frac_mask  0x7fffff
#define Frac_mask1 0xffff007f
#define Ten_pmax 24
#define Bletch 2
#define Bndry_mask  0xffff007f
#define Bndry_mask1 0xffff007f
#define LSB 0x10000
#define Sign_bit 0x8000
#define Log2P 1
#define Tiny0 0x80
#define Tiny1 0
#define Quick_max 15
#define Int_max 15
#endif /* IBM, VAX */
#endif /* IEEE_Arith */

#ifndef IEEE_Arith
#define ROUND_BIASED
#else
#ifdef ROUND_BIASED_without_Round_Up
#undef  ROUND_BIASED
#define ROUND_BIASED
#endif
#endif

#ifdef RND_PRODQUOT
#define rounded_product(a,b) a = rnd_prod(a, b)
#define rounded_quotient(a,b) a = rnd_quot(a, b)
extern double rnd_prod(double, double), rnd_quot(double, double);
#else
#define rounded_product(a,b) a *= b
#define rounded_quotient(a,b) a /= b
#endif

#define Big0 (Frac_mask1 | Exp_msk1*(DBL_MAX_EXP+Bias-1))
#define Big1 0xffffffff

#ifndef Pack_32
#define Pack_32
#endif

typedef struct BCinfo BCinfo;
 struct
BCinfo { int dp0, dp1, dplen, dsign, e0, inexact, nd, nd0, rounding, scale, uflchk; };

#define FFFFFFFF 0xffffffffUL

#ifdef MULTIPLE_THREADS
#define MTa , PTI
#define MTb , &TI
#define MTd , ThInfo **PTI
#else
#define MTa /*nothing*/
#define MTb /*nothing*/
#define MTd /*nothing*/
#endif

#define Kmax 7

struct
Bigint {
	struct Bigint *next;
	int k, maxwds, sign, wds;
	ULong x[1];
};

typedef struct Bigint Bigint;
typedef struct
ThInfo {
	Bigint *Freelist[Kmax+1];
	Bigint *P5s;
} ThInfo;


__CPP_BEGIN
extern Bigint *__Balloc(int k MTd);
extern void __Bfree(Bigint *v MTd);

#define Bcopy(x,y) memcpy((char *)&x->sign, (char *)&y->sign, \
y->wds*sizeof(Long) + 2*sizeof(int))

extern Bigint *__d2b(U *d, int *e, int *bits MTd);
extern Bigint *__i2b(int i MTd);
extern Bigint *__pow5mult(Bigint *b, int k MTd);
extern Bigint *__mult(Bigint *a, Bigint *b MTd);
extern Bigint *__multadd(Bigint *b, int m, int a MTd);
extern int __quorem(Bigint *b, Bigint *S);
extern Bigint *__diff(Bigint *a, Bigint *b MTd);

extern int __dshift(Bigint *b, int p2);
extern Bigint *__lshift(Bigint *b, int k MTd);

extern int __hi0bits(ULong x);

extern int __cmp(Bigint *a, Bigint *b);

__CPP_END

#endif /* __DTOA_H */
