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

#include "__dtoa.h"
 
 static Bigint *
s2b(const char *s, int nd0, int nd, ULong y9, int dplen MTd)
{
	Bigint *b;
	int i, k;
	Long x, y;

	x = (nd + 8) / 9;
	for(k = 0, y = 1; x > y; y <<= 1, k++) ;
#ifdef Pack_32
	b = __Balloc(k MTa);
	b->x[0] = y9;
	b->wds = 1;
#else
	b = __Balloc(k+1 MTa);
	b->x[0] = y9 & 0xffff;
	b->wds = (b->x[1] = y9 >> 16) ? 2 : 1;
#endif

	i = 9;
	if (9 < nd0) {
		s += 9;
		do b = __multadd(b, 10, *s++ - '0' MTa);
			while(++i < nd0);
		s += dplen;
		}
	else
		s += dplen + 9;
	for(; i < nd; i++)
		b = __multadd(b, 10, *s++ - '0' MTa);
	return b;
	}

 static double
ulp(U *x)
{
	Long L;
	U u;

	L = (word0(x) & Exp_mask) - (P-1)*Exp_msk1;
#ifndef Avoid_Underflow
#ifndef Sudden_Underflow
	if (L > 0) {
#endif
#endif
#ifdef IBM
		L |= Exp_msk1 >> 4;
#endif
		word0(&u) = L;
		word1(&u) = 0;
#ifndef Avoid_Underflow
#ifndef Sudden_Underflow
		}
	else {
		L = -L >> Exp_shift;
		if (L < Exp_shift) {
			word0(&u) = 0x80000 >> L;
			word1(&u) = 0;
			}
		else {
			word0(&u) = 0;
			L -= Exp_shift;
			word1(&u) = L >= 31 ? 1 : 1 << 31 - L;
			}
		}
#endif
#endif
	return dval(&u);
	}

 static double
b2d(Bigint *a, int *e)
{
	ULong *xa, *xa0, w, y, z;
	int k;
	U d;
#ifdef VAX
	ULong d0, d1;
#else
#define d0 word0(&d)
#define d1 word1(&d)
#endif

	xa0 = a->x;
	xa = xa0 + a->wds;
	y = *--xa;
#ifdef DTOA_DEBUG
	if (!y) Bug("zero y in b2d");
#endif
	k = __hi0bits(y);
	*e = 32 - k;
#ifdef Pack_32
	if (k < Ebits) {
		d0 = Exp_1 | y >> (Ebits - k);
		w = xa > xa0 ? *--xa : 0;
		d1 = y << ((32-Ebits) + k) | w >> (Ebits - k);
		goto ret_d;
		}
	z = xa > xa0 ? *--xa : 0;
	if (k -= Ebits) {
		d0 = Exp_1 | y << k | z >> (32 - k);
		y = xa > xa0 ? *--xa : 0;
		d1 = z << k | y >> (32 - k);
		}
	else {
		d0 = Exp_1 | y;
		d1 = z;
		}
#else
	if (k < Ebits + 16) {
		z = xa > xa0 ? *--xa : 0;
		d0 = Exp_1 | y << k - Ebits | z >> Ebits + 16 - k;
		w = xa > xa0 ? *--xa : 0;
		y = xa > xa0 ? *--xa : 0;
		d1 = z << k + 16 - Ebits | w << k - Ebits | y >> 16 + Ebits - k;
		goto ret_d;
		}
	z = xa > xa0 ? *--xa : 0;
	w = xa > xa0 ? *--xa : 0;
	k -= Ebits + 16;
	d0 = Exp_1 | y << k + 16 | z << k | w >> 16 - k;
	y = xa > xa0 ? *--xa : 0;
	d1 = w << k + 16 | y << k;
#endif
 ret_d:
#ifdef VAX
	word0(&d) = d0 >> 16 | d0 << 16;
	word1(&d) = d1 >> 16 | d1 << 16;
#else
#undef d0
#undef d1
#endif
	return dval(&d);
	}

 static double
ratio(Bigint *a, Bigint *b)
{
	U da, db;
	int k, ka, kb;

	dval(&da) = b2d(a, &ka);
	dval(&db) = b2d(b, &kb);
#ifdef Pack_32
	k = ka - kb + 32*(a->wds - b->wds);
#else
	k = ka - kb + 16*(a->wds - b->wds);
#endif
#ifdef IBM
	if (k > 0) {
		word0(&da) += (k >> 2)*Exp_msk1;
		if (k &= 3)
			dval(&da) *= 1 << k;
		}
	else {
		k = -k;
		word0(&db) += (k >> 2)*Exp_msk1;
		if (k &= 3)
			dval(&db) *= 1 << k;
		}
#else
	if (k > 0)
		word0(&da) += k*Exp_msk1;
	else {
		k = -k;
		word0(&db) += k*Exp_msk1;
		}
#endif
	return dval(&da) / dval(&db);
	}

 static const double
tens[] = {
		1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
		1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
		1e20, 1e21, 1e22
#ifdef VAX
		, 1e23, 1e24
#endif
		};

 static const double
#ifdef IEEE_Arith
bigtens[] = { 1e16, 1e32, 1e64, 1e128, 1e256 };
static const double tinytens[] = { 1e-16, 1e-32, 1e-64, 1e-128,
#ifdef Avoid_Underflow
		9007199254740992.*9007199254740992.e-256
		/* = 2^106 * 1e-256 */
#else
		1e-256
#endif
		};
/* The factor of 2^53 in tinytens[4] helps us avoid setting the underflow */
/* flag unnecessarily.  It leads to a song and dance at the end of strtod. */
#define Scale_Bit 0x10
#define n_bigtens 5
#else
#ifdef IBM
bigtens[] = { 1e16, 1e32, 1e64 };
static const double tinytens[] = { 1e-16, 1e-32, 1e-64 };
#define n_bigtens 3
#else
bigtens[] = { 1e16, 1e32 };
static const double tinytens[] = { 1e-16, 1e-32 };
#define n_bigtens 2
#endif
#endif

#undef Need_Hexdig
#ifdef INFNAN_CHECK
#ifndef No_Hex_NaN
#define Need_Hexdig
#endif
#endif

#ifndef Need_Hexdig
#ifndef NO_HEX_FP
#define Need_Hexdig
#endif
#endif

#ifdef Need_Hexdig /*{*/
#if 0
static unsigned char hexdig[256];

 static void
htinit(unsigned char *h, unsigned char *s, int inc)
{
	int i, j;
	for(i = 0; (j = s[i]) !=0; i++)
		h[j] = i + inc;
	}

 static void
hexdig_init(void)	/* Use of hexdig_init omitted 20121220 to avoid a */
			/* race condition when multiple threads are used. */
{
#define USC (unsigned char *)
	htinit(hexdig, USC "0123456789", 0x10);
	htinit(hexdig, USC "abcdef", 0x10 + 10);
	htinit(hexdig, USC "ABCDEF", 0x10 + 10);
	}
#else
static unsigned char hexdig[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	16,17,18,19,20,21,22,23,24,25,0,0,0,0,0,0,
	0,26,27,28,29,30,31,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,26,27,28,29,30,31,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	};
#endif
#endif /* } Need_Hexdig */

#ifdef INFNAN_CHECK

#ifndef NAN_WORD0
#define NAN_WORD0 0x7ff80000
#endif

#ifndef NAN_WORD1
#define NAN_WORD1 0
#endif

 static int
match(const char **sp, const char *t)
{
	int c, d;
	const char *s = *sp;

	while((d = *t++)) {
		if ((c = *++s) >= 'A' && c <= 'Z')
			c += 'a' - 'A';
		if (c != d)
			return 0;
		}
	*sp = s + 1;
	return 1;
	}

#ifndef No_Hex_NaN
 static void
hexnan(U *rvp, const char **sp)
{
	ULong c, x[2];
	const char *s;
	int c1, havedig, udx0, xshift;

	/**** if (!hexdig['0']) hexdig_init(); ****/
	x[0] = x[1] = 0;
	havedig = xshift = 0;
	udx0 = 1;
	s = *sp;
	/* allow optional initial 0x or 0X */
	while((c = *(const unsigned char*)(s+1)) && c <= ' ')
		++s;
	if (s[1] == '0' && (s[2] == 'x' || s[2] == 'X'))
		s += 2;
	while((c = *(const unsigned char*)++s)) {
		if ((c1 = hexdig[c]))
			c  = c1 & 0xf;
		else if (c <= ' ') {
			if (udx0 && havedig) {
				udx0 = 0;
				xshift = 1;
				}
			continue;
			}
#ifdef GDTOA_NON_PEDANTIC_NANCHECK
		else if (/*(*/ c == ')' && havedig) {
			*sp = s + 1;
			break;
			}
		else
			return;	/* invalid form: don't change *sp */
#else
		else {
			do {
				if (/*(*/ c == ')') {
					*sp = s + 1;
					break;
					}
				} while((c = *++s));
			break;
			}
#endif
		havedig = 1;
		if (xshift) {
			xshift = 0;
			x[0] = x[1];
			x[1] = 0;
			}
		if (udx0)
			x[0] = (x[0] << 4) | (x[1] >> 28);
		x[1] = (x[1] << 4) | c;
		}
	if ((x[0] &= 0xfffff) || x[1]) {
		word0(rvp) = Exp_mask | x[0];
		word1(rvp) = x[1];
		}
	}
#endif /*No_Hex_NaN*/
#endif /* INFNAN_CHECK */

#ifdef Pack_32
#define ULbits 32
#define kshift 5
#define kmask 31
#else
#define ULbits 16
#define kshift 4
#define kmask 15
#endif

#if !defined(NO_HEX_FP) || defined(Honor_FLT_ROUNDS) /*{*/
 static Bigint *
increment(Bigint *b MTd)
{
	ULong *x, *xe;
	Bigint *b1;

	x = b->x;
	xe = x + b->wds;
	do {
		if (*x < (ULong)0xffffffffL) {
			++*x;
			return b;
			}
		*x++ = 0;
		} while(x < xe);
	{
		if (b->wds >= b->maxwds) {
			b1 = __Balloc(b->k+1 MTa);
			Bcopy(b1,b);
			__Bfree(b MTa);
			b = b1;
			}
		b->x[b->wds++] = 1;
		}
	return b;
	}

#endif /*}*/

#ifndef NO_HEX_FP /*{*/

 static void
rshift(Bigint *b, int k)
{
	ULong *x, *x1, *xe, y;
	int n;

	x = x1 = b->x;
	n = k >> kshift;
	if (n < b->wds) {
		xe = x + b->wds;
		x += n;
		if (k &= kmask) {
			n = 32 - k;
			y = *x++ >> k;
			while(x < xe) {
				*x1++ = (y | (*x << n)) & 0xffffffff;
				y = *x++ >> k;
				}
			if ((*x1 = y) !=0)
				x1++;
			}
		else
			while(x < xe)
				*x1++ = *x++;
		}
	if ((b->wds = x1 - b->x) == 0)
		b->x[0] = 0;
	}

 static ULong
any_on(Bigint *b, int k)
{
	int n, nwds;
	ULong *x, *x0, x1, x2;

	x = b->x;
	nwds = b->wds;
	n = k >> kshift;
	if (n > nwds)
		n = nwds;
	else if (n < nwds && (k &= kmask)) {
		x1 = x2 = x[n];
		x1 >>= k;
		x1 <<= k;
		if (x1 != x2)
			return 1;
		}
	x0 = x;
	x += n;
	while(x > x0)
		if (*--x)
			return 1;
	return 0;
	}

enum {	/* rounding values: same as FLT_ROUNDS */
	Round_zero = 0,
	Round_near = 1,
	Round_up = 2,
	Round_down = 3
	};

 void
gethex(const char **sp, U *rvp, int rounding, int sign MTd)
{
	Bigint *b;
	char d;
	const unsigned char *decpt, *s0, *s, *s1;
	Long e, e1;
	ULong L, lostbits, *x;
	int big, denorm, esign, havedig, k, n, nb, nbits, nz, up, zret;
#ifdef IBM
	int j;
#endif
	enum {
#ifdef IEEE_Arith /*{{*/
		emax = 0x7fe - Bias - P + 1,
		emin = Emin - P + 1
#else /*}{*/
		emin = Emin - P,
#ifdef VAX
		emax = 0x7ff - Bias - P + 1
#endif
#ifdef IBM
		emax = 0x7f - Bias - P
#endif
#endif /*}}*/
		};
#ifdef IEEE_Arith
	int check_denorm = 0;
#endif
#ifdef USE_LOCALE
	int i;
#ifdef NO_LOCALE_CACHE
	const unsigned char *decimalpoint = (unsigned char*)
		localeconv()->decimal_point;
#else
	const unsigned char *decimalpoint;
	static unsigned char *decimalpoint_cache;
	if (!(s0 = decimalpoint_cache)) {
		s0 = (unsigned char*)localeconv()->decimal_point;
		if ((decimalpoint_cache = (unsigned char*)
				MALLOC(strlen((const char*)s0) + 1))) {
			strcpy((char*)decimalpoint_cache, (const char*)s0);
			s0 = decimalpoint_cache;
			}
		}
	decimalpoint = s0;
#endif
#endif

	/**** if (!hexdig['0']) hexdig_init(); ****/
	havedig = 0;
	s0 = *(const unsigned char **)sp + 2;
	while(s0[havedig] == '0')
		havedig++;
	s0 += havedig;
	s = s0;
	decpt = 0;
	zret = 0;
	e = 0;
	if (hexdig[*s])
		havedig++;
	else {
		zret = 1;
#ifdef USE_LOCALE
		for(i = 0; decimalpoint[i]; ++i) {
			if (s[i] != decimalpoint[i])
				goto pcheck;
			}
		decpt = s += i;
#else
		if (*s != '.')
			goto pcheck;
		decpt = ++s;
#endif
		if (!hexdig[*s])
			goto pcheck;
		while(*s == '0')
			s++;
		if (hexdig[*s])
			zret = 0;
		havedig = 1;
		s0 = s;
		}
	while(hexdig[*s])
		s++;
#ifdef USE_LOCALE
	if (*s == *decimalpoint && !decpt) {
		for(i = 1; decimalpoint[i]; ++i) {
			if (s[i] != decimalpoint[i])
				goto pcheck;
			}
		decpt = s += i;
#else
	if (*s == '.' && !decpt) {
		decpt = ++s;
#endif
		while(hexdig[*s])
			s++;
		}/*}*/
	if (decpt)
		e = -(((Long)(s-decpt)) << 2);
 pcheck:
	s1 = s;
	big = esign = 0;
	switch(*s) {
	  case 'p':
	  case 'P':
		switch(*++s) {
		  case '-':
			esign = 1;
			/* no break */
		  case '+':
			s++;
		  }
		if ((n = hexdig[*s]) == 0 || n > 0x19) {
			s = s1;
			break;
			}
		e1 = n - 0x10;
		while((n = hexdig[*++s]) !=0 && n <= 0x19) {
			if (e1 & 0xf8000000)
				big = 1;
			e1 = 10*e1 + n - 0x10;
			}
		if (esign)
			e1 = -e1;
		e += e1;
	  }
	*sp = (char*)s;
	if (!havedig)
		*sp = (char*)s0 - 1;
	if (zret)
		goto retz1;
	if (big) {
		if (esign) {
#ifdef IEEE_Arith
			switch(rounding) {
			  case Round_up:
				if (sign)
					break;
				goto ret_tiny;
			  case Round_down:
				if (!sign)
					break;
				goto ret_tiny;
			  }
#endif
			goto retz;
#ifdef IEEE_Arith
 ret_tinyf:
			__Bfree(b MTa);
 ret_tiny:
			Set_errno(ERANGE);
			word0(rvp) = 0;
			word1(rvp) = 1;
			return;
#endif /* IEEE_Arith */
			}
		switch(rounding) {
		  case Round_near:
			goto ovfl1;
		  case Round_up:
			if (!sign)
				goto ovfl1;
			goto ret_big;
		  case Round_down:
			if (sign)
				goto ovfl1;
			goto ret_big;
		  }
 ret_big:
		word0(rvp) = Big0;
		word1(rvp) = Big1;
		return;
		}
	n = s1 - s0 - 1;
	for(k = 0; n > (1 << (kshift-2)) - 1; n >>= 1)
		k++;
	b = __Balloc(k MTa);
	x = b->x;
	havedig = n = nz = 0;
	L = 0;
#ifdef USE_LOCALE
	for(i = 0; decimalpoint[i+1]; ++i);
#endif
	while(s1 > s0) {
#ifdef USE_LOCALE
		if (*--s1 == decimalpoint[i]) {
			s1 -= i;
			continue;
			}
#else
		if (*--s1 == '.')
			continue;
#endif
		if ((d = hexdig[*s1]))
			havedig = 1;
		else if (!havedig) {
			e += 4;
			continue;
			}
		if (n == ULbits) {
			*x++ = L;
			L = 0;
			n = 0;
			}
		L |= (d & 0x0f) << n;
		n += 4;
		}
	*x++ = L;
	b->wds = n = x - b->x;
	nb = ULbits*n - __hi0bits(L);
	nbits = Nbits;
	lostbits = 0;
	x = b->x;
	if (nb > nbits) {
		n = nb - nbits;
		if (any_on(b,n)) {
			lostbits = 1;
			k = n - 1;
			if (x[k>>kshift] & 1 << (k & kmask)) {
				lostbits = 2;
				if (k > 0 && any_on(b,k))
					lostbits = 3;
				}
			}
		rshift(b, n);
		e += n;
		}
	else if (nb < nbits) {
		n = nbits - nb;
		b = __lshift(b, n MTa);
		e -= n;
		x = b->x;
		}
	if (e > emax) {
 ovfl:
		__Bfree(b MTa);
 ovfl1:
		Set_errno(ERANGE);
#ifdef Honor_FLT_ROUNDS
		switch (rounding) {
		  case Round_zero:
			goto ret_big;
		  case Round_down:
			if (!sign)
				goto ret_big;
			break;
		  case Round_up:
			if (sign)
				goto ret_big;
		  }
#endif
		word0(rvp) = Exp_mask;
		word1(rvp) = 0;
		return;
		}
	denorm = 0;
	if (e < emin) {
		denorm = 1;
		n = emin - e;
		if (n >= nbits) {
#ifdef IEEE_Arith /*{*/
			switch (rounding) {
			  case Round_near:
				if (n == nbits && (n < 2 || lostbits || any_on(b,n-1)))
					goto ret_tinyf;
				break;
			  case Round_up:
				if (!sign)
					goto ret_tinyf;
				break;
			  case Round_down:
				if (sign)
					goto ret_tinyf;
			  }
#endif /* } IEEE_Arith */
			__Bfree(b MTa);
 retz:
			Set_errno(ERANGE);
 retz1:
			rvp->d = 0.;
			return;
			}
		k = n - 1;
#ifdef IEEE_Arith
		if (!k) {
			switch(rounding) {
			  case Round_near:
				if (((b->x[0] & 3) == 3) || (lostbits && (b->x[0] & 1))) {
					__multadd(b, 1, 1 MTa);
 emin_check:
					if (b->x[1] == (1 << (Exp_shift + 1))) {
						rshift(b,1);
						e = emin;
						goto normal;
						}
					}
				break;
			  case Round_up:
				if (!sign && (lostbits || (b->x[0] & 1))) {
 incr_denorm:
					__multadd(b, 1, 2 MTa);
					check_denorm = 1;
					lostbits = 0;
					goto emin_check;
					}
				break;
			  case Round_down:
				if (sign && (lostbits || (b->x[0] & 1)))
					goto incr_denorm;
				break;
			  }
			}
#endif
		if (lostbits)
			lostbits = 1;
		else if (k > 0)
			lostbits = any_on(b,k);
#ifdef IEEE_Arith
		else if (check_denorm)
			goto no_lostbits;
#endif
		if (x[k>>kshift] & 1 << (k & kmask))
			lostbits |= 2;
#ifdef IEEE_Arith
 no_lostbits:
#endif
		nbits -= n;
		rshift(b,n);
		e = emin;
		}
	if (lostbits) {
		up = 0;
		switch(rounding) {
		  case Round_zero:
			break;
		  case Round_near:
			if (lostbits & 2
			 && (lostbits & 1) | (x[0] & 1))
				up = 1;
			break;
		  case Round_up:
			up = 1 - sign;
			break;
		  case Round_down:
			up = sign;
		  }
		if (up) {
			k = b->wds;
			b = increment(b MTa);
			x = b->x;
			if (!denorm && (b->wds > k
			 || ((n = nbits & kmask) !=0
			     && __hi0bits(x[k-1]) < 32-n))) {
				rshift(b,1);
				if (++e > Emax)
					goto ovfl;
				}
			}
		}
#ifdef IEEE_Arith
	if (denorm)
		word0(rvp) = b->wds > 1 ? b->x[1] & ~0x100000 : 0;
	else {
 normal:
		word0(rvp) = (b->x[1] & ~0x100000) | ((e + 0x3ff + 52) << 20);
		}
	word1(rvp) = b->x[0];
#endif
#ifdef IBM
	if ((j = e & 3)) {
		k = b->x[0] & ((1 << j) - 1);
		rshift(b,j);
		if (k) {
			switch(rounding) {
			  case Round_up:
				if (!sign)
					increment(b);
				break;
			  case Round_down:
				if (sign)
					increment(b);
				break;
			  case Round_near:
				j = 1 << (j-1);
				if (k & j && ((k & (j-1)) | lostbits))
					increment(b);
			  }
			}
		}
	e >>= 2;
	word0(rvp) = b->x[1] | ((e + 65 + 13) << 24);
	word1(rvp) = b->x[0];
#endif
#ifdef VAX
	/* The next two lines ignore swap of low- and high-order 2 bytes. */
	/* word0(rvp) = (b->x[1] & ~0x800000) | ((e + 129 + 55) << 23); */
	/* word1(rvp) = b->x[0]; */
	word0(rvp) = ((b->x[1] & ~0x800000) >> 16) | ((e + 129 + 55) << 7) | (b->x[1] << 16);
	word1(rvp) = (b->x[0] >> 16) | (b->x[0] << 16);
#endif
	__Bfree(b MTa);
	}
#endif /*!NO_HEX_FP}*/

#if defined(Avoid_Underflow) || !defined(NO_STRTOD_BIGCOMP) /*{*/
 static double
sulp(U *x, BCinfo *bc)
{
	U u;
	double rv;
	int i;

	rv = ulp(x);
	if (!bc->scale || (i = 2*P + 1 - ((word0(x) & Exp_mask) >> Exp_shift)) <= 0)
		return rv; /* Is there an example where i <= 0 ? */
	word0(&u) = Exp_1 + (i << Exp_shift);
	word1(&u) = 0;
	return rv * u.d;
	}
#endif /*}*/

#ifndef NO_STRTOD_BIGCOMP
 static void
bigcomp(U *rv, const char *s0, BCinfo *bc MTd)
{
	Bigint *b, *d;
	int b2, bbits, d2, dd, dig, dsign, i, j, nd, nd0, p2, p5, speccase;

	dsign = bc->dsign;
	nd = bc->nd;
	nd0 = bc->nd0;
	p5 = nd + bc->e0 - 1;
	speccase = 0;
#ifndef Sudden_Underflow
	if (rv->d == 0.) {	/* special case: value near underflow-to-zero */
				/* threshold was rounded to zero */
		b = __i2b(1 MTa);
		p2 = Emin - P + 1;
		bbits = 1;
#ifdef Avoid_Underflow
		word0(rv) = (P+2) << Exp_shift;
#else
		word1(rv) = 1;
#endif
		i = 0;
#ifdef Honor_FLT_ROUNDS
		if (bc->rounding == 1)
#endif
			{
			speccase = 1;
			--p2;
			dsign = 0;
			goto have_i;
			}
		}
	else
#endif
		b = __d2b(rv, &p2, &bbits MTa);
#ifdef Avoid_Underflow
	p2 -= bc->scale;
#endif
	/* floor(log2(rv)) == bbits - 1 + p2 */
	/* Check for denormal case. */
	i = P - bbits;
	if (i > (j = P - Emin - 1 + p2)) {
#ifdef Sudden_Underflow
		__Bfree(b MTa);
		b = __i2b(1 MTa);
		p2 = Emin;
		i = P - 1;
#ifdef Avoid_Underflow
		word0(rv) = (1 + bc->scale) << Exp_shift;
#else
		word0(rv) = Exp_msk1;
#endif
		word1(rv) = 0;
#else
		i = j;
#endif
		}
#ifdef Honor_FLT_ROUNDS
	if (bc->rounding != 1) {
		if (i > 0)
			b = __lshift(b, i MTa);
		if (dsign)
			b = increment(b MTa);
		}
	else
#endif
		{
		b = __lshift(b, ++i MTa);
		b->x[0] |= 1;
		}
#ifndef Sudden_Underflow
 have_i:
#endif
	p2 -= p5 + i;
	d = __i2b(1 MTa);
	/* Arrange for convenient computation of quotients:
	 * shift left if necessary so divisor has 4 leading 0 bits.
	 */
	if (p5 > 0)
		d = __pow5mult(d, p5 MTa);
	else if (p5 < 0)
		b = __pow5mult(b, -p5 MTa);
	if (p2 > 0) {
		b2 = p2;
		d2 = 0;
		}
	else {
		b2 = 0;
		d2 = -p2;
		}
	i = __dshift(d, d2);
	if ((b2 += i) > 0)
		b = __lshift(b, b2 MTa);
	if ((d2 += i) > 0)
		d = __lshift(d, d2 MTa);

	/* Now b/d = exactly half-way between the two floating-point values */
	/* on either side of the input string.  Compute first digit of b/d. */

	if (!(dig = __quorem(b,d))) {
		b = __multadd(b, 10, 0 MTa);	/* very unlikely */
		dig = __quorem(b,d);
		}

	/* Compare b/d with s0 */

	for(i = 0; i < nd0; ) {
		if ((dd = s0[i++] - '0' - dig))
			goto ret;
		if (!b->x[0] && b->wds == 1) {
			if (i < nd)
				dd = 1;
			goto ret;
			}
		b = __multadd(b, 10, 0 MTa);
		dig = __quorem(b,d);
		}
	for(j = bc->dp1; i++ < nd;) {
		if ((dd = s0[j++] - '0' - dig))
			goto ret;
		if (!b->x[0] && b->wds == 1) {
			if (i < nd)
				dd = 1;
			goto ret;
			}
		b = __multadd(b, 10, 0 MTa);
		dig = __quorem(b,d);
		}
	if (dig > 0 || b->x[0] || b->wds > 1)
		dd = -1;
 ret:
	__Bfree(b MTa);
	__Bfree(d MTa);
#ifdef Honor_FLT_ROUNDS
	if (bc->rounding != 1) {
		if (dd < 0) {
			if (bc->rounding == 0) {
				if (!dsign)
					goto retlow1;
				}
			else if (dsign)
				goto rethi1;
			}
		else if (dd > 0) {
			if (bc->rounding == 0) {
				if (dsign)
					goto rethi1;
				goto ret1;
				}
			if (!dsign)
				goto rethi1;
			dval(rv) += 2.*sulp(rv,bc);
			}
		else {
			bc->inexact = 0;
			if (dsign)
				goto rethi1;
			}
		}
	else
#endif
	if (speccase) {
		if (dd <= 0)
			rv->d = 0.;
		}
	else if (dd < 0) {
		if (!dsign)	/* does not happen for round-near */
retlow1:
			dval(rv) -= sulp(rv,bc);
		}
	else if (dd > 0) {
		if (dsign) {
 rethi1:
			dval(rv) += sulp(rv,bc);
			}
		}
	else {
		/* Exact half-way case:  apply round-even rule. */
		if ((j = ((word0(rv) & Exp_mask) >> Exp_shift) - bc->scale) <= 0) {
			i = 1 - j;
			if (i <= 31) {
				if (word1(rv) & (0x1 << i))
					goto odd;
				}
			else if (word0(rv) & (0x1 << (i-32)))
				goto odd;
			}
		else if (word1(rv) & 1) {
 odd:
			if (dsign)
				goto rethi1;
			goto retlow1;
			}
		}

#ifdef Honor_FLT_ROUNDS
 ret1:
#endif
	return;
	}
#endif /* NO_STRTOD_BIGCOMP */


 double
strtod(const char * _Restrict s00, char ** _Restrict se)
{
	int bb2, bb5, bbe, bd2, bd5, bbbits, bs2, c, e, e1;
	int esign, i, j, k, nd, nd0, nf, nz, nz0, nz1, sign;
	const char *s, *s0, *s1;
	double aadj, aadj1;
	Long L;
	U aadj2, adj, rv, rv0;
	ULong y, z;
	BCinfo bc;
	Bigint *bb, *bb1, *bd, *bd0, *bs, *delta;
#ifdef USE_BF96
	ULLong bhi, blo, brv, t00, t01, t02, t10, t11, terv, tg, tlo, yz;
	const BF96 *p10;
	int bexact, erv;
#endif
#ifdef Avoid_Underflow
	ULong Lsb, Lsb1;
#endif
#ifdef SET_INEXACT
	int oldinexact;
#endif
#ifndef NO_STRTOD_BIGCOMP
	int req_bigcomp = 0;
#endif
#ifdef MULTIPLE_THREADS
	ThInfo *TI = 0;
#endif
#ifdef Honor_FLT_ROUNDS /*{*/
#ifdef Trust_FLT_ROUNDS /*{{ only define this if FLT_ROUNDS really works! */
	bc.rounding = Flt_Rounds;
#else /*}{*/
	bc.rounding = 1;
	switch(fegetround()) {
	  case FE_TOWARDZERO:	bc.rounding = 0; break;
	  case FE_UPWARD:	bc.rounding = 2; break;
	  case FE_DOWNWARD:	bc.rounding = 3;
	  }
#endif /*}}*/
#endif /*}*/
#ifdef USE_LOCALE
	const char *s2;
#endif

	sign = nz0 = nz1 = nz = bc.dplen = bc.uflchk = 0;
	dval(&rv) = 0.;
	for(s = s00;;s++) switch(*s) {
		case '-':
			sign = 1;
			/* no break */
		case '+':
			if (*++s)
				goto break2;
			/* no break */
		case 0:
			goto ret0;
		case '\t':
		case '\n':
		case '\v':
		case '\f':
		case '\r':
		case ' ':
			continue;
		default:
			goto break2;
		}
 break2:
	if (*s == '0') {
#ifndef NO_HEX_FP /*{*/
		switch(s[1]) {
		  case 'x':
		  case 'X':
#ifdef Honor_FLT_ROUNDS
			gethex(&s, &rv, bc.rounding, sign MTb);
#else
			gethex(&s, &rv, 1, sign MTb);
#endif
			goto ret;
		  }
#endif /*}*/
		nz0 = 1;
		while(*++s == '0') ;
		if (!*s)
			goto ret;
		}
	s0 = s;
	nd = nf = 0;
#ifdef USE_BF96
	yz = 0;
	for(; (c = *s) >= '0' && c <= '9'; nd++, s++)
		if (nd < 19)
			yz = 10*yz + c - '0';
#else
	y = z = 0;
	for(; (c = *s) >= '0' && c <= '9'; nd++, s++)
		if (nd < 9)
			y = 10*y + c - '0';
		else if (nd < DBL_DIG + 2)
			z = 10*z + c - '0';
#endif
	nd0 = nd;
	bc.dp0 = bc.dp1 = s - s0;
	for(s1 = s; s1 > s0 && *--s1 == '0'; )
		++nz1;
#ifdef USE_LOCALE
	s1 = localeconv()->decimal_point;
	if (c == *s1) {
		c = '.';
		if (*++s1) {
			s2 = s;
			for(;;) {
				if (*++s2 != *s1) {
					c = 0;
					break;
					}
				if (!*++s1) {
					s = s2;
					break;
					}
				}
			}
		}
#endif
	if (c == '.') {
		c = *++s;
		bc.dp1 = s - s0;
		bc.dplen = bc.dp1 - bc.dp0;
		if (!nd) {
			for(; c == '0'; c = *++s)
				nz++;
			if (c > '0' && c <= '9') {
				bc.dp0 = s0 - s;
				bc.dp1 = bc.dp0 + bc.dplen;
				s0 = s;
				nf += nz;
				nz = 0;
				goto have_dig;
				}
			goto dig_done;
			}
		for(; c >= '0' && c <= '9'; c = *++s) {
 have_dig:
			nz++;
			if (c -= '0') {
				nf += nz;
				i = 1;
#ifdef USE_BF96
				for(; i < nz; ++i) {
					if (++nd <= 19)
						yz *= 10;
					}
				if (++nd <= 19)
					yz = 10*yz + c;
#else
				for(; i < nz; ++i) {
					if (nd++ < 9)
						y *= 10;
					else if (nd <= DBL_DIG + 2)
						z *= 10;
					}
				if (nd++ < 9)
					y = 10*y + c;
				else if (nd <= DBL_DIG + 2)
					z = 10*z + c;
#endif
				nz = nz1 = 0;
				}
			}
		}
 dig_done:
	e = 0;
	if (c == 'e' || c == 'E') {
		if (!nd && !nz && !nz0) {
			goto ret0;
			}
		s00 = s;
		esign = 0;
		switch(c = *++s) {
			case '-':
				esign = 1;
			case '+':
				c = *++s;
			}
		if (c >= '0' && c <= '9') {
			while(c == '0')
				c = *++s;
			if (c > '0' && c <= '9') {
				L = c - '0';
				while((c = *++s) >= '0' && c <= '9') {
					if (L <= 19999)
						L = 10*L + c - '0';
					}
				if (L > 19999)
					/* Avoid confusion from exponents
					 * so large that e might overflow.
					 */
					e = 19999; /* safe for 16 bit ints */
				else
					e = (int)L;
				if (esign)
					e = -e;
				}
			else
				e = 0;
			}
		else
			s = s00;
		}
	if (!nd) {
		if (!nz && !nz0) {
#ifdef INFNAN_CHECK /*{*/
			/* Check for Nan and Infinity */
			if (!bc.dplen)
			 switch(c) {
			  case 'i':
			  case 'I':
				if (match(&s,"nf")) {
					--s;
					if (!match(&s,"inity"))
						++s;
					word0(&rv) = 0x7ff00000;
					word1(&rv) = 0;
					goto ret;
					}
				break;
			  case 'n':
			  case 'N':
				if (match(&s, "an")) {
					word0(&rv) = NAN_WORD0;
					word1(&rv) = NAN_WORD1;
#ifndef No_Hex_NaN
					if (*s == '(') /*)*/
						hexnan(&rv, &s);
#endif
					goto ret;
					}
			  }
#endif /*} INFNAN_CHECK */
 ret0:
			s = s00;
			sign = 0;
			}
		goto ret;
		}
	bc.e0 = e1 = e -= nf;

	/* Now we have nd0 digits, starting at s0, followed by a
	 * decimal point, followed by nd-nd0 digits.  The number we're
	 * after is the integer represented by those digits times
	 * 10**e */

	if (!nd0)
		nd0 = nd;
#ifndef USE_BF96
	k = nd < DBL_DIG + 2 ? nd : DBL_DIG + 2;
	dval(&rv) = y;
	if (k > 9) {
#ifdef SET_INEXACT
		if (k > DBL_DIG)
			oldinexact = get_inexact();
#endif
		dval(&rv) = tens[k - 9] * dval(&rv) + z;
		}
#endif
	bd0 = 0;
	if (nd <= DBL_DIG
#ifndef RND_PRODQUOT
#ifndef Honor_FLT_ROUNDS
		&& Flt_Rounds == 1
#endif
#endif
			) {
#ifdef USE_BF96
		dval(&rv) = yz;
#endif
		if (!e)
			goto ret;
#ifndef ROUND_BIASED_without_Round_Up
		if (e > 0) {
			if (e <= Ten_pmax) {
#ifdef SET_INEXACT
				bc.inexact = 0;
				oldinexact = 1;
#endif
#ifdef VAX
				goto vax_ovfl_check;
#else
#ifdef Honor_FLT_ROUNDS
				/* round correctly FLT_ROUNDS = 2 or 3 */
				if (sign) {
					rv.d = -rv.d;
					sign = 0;
					}
#endif
				/* rv = */ rounded_product(dval(&rv), tens[e]);
				goto ret;
#endif
				}
			i = DBL_DIG - nd;
			if (e <= Ten_pmax + i) {
				/* A fancier test would sometimes let us do
				 * this for larger i values.
				 */
#ifdef SET_INEXACT
				bc.inexact = 0;
				oldinexact = 1;
#endif
#ifdef Honor_FLT_ROUNDS
				/* round correctly FLT_ROUNDS = 2 or 3 */
				if (sign) {
					rv.d = -rv.d;
					sign = 0;
					}
#endif
				e -= i;
				dval(&rv) *= tens[i];
#ifdef VAX
				/* VAX exponent range is so narrow we must
				 * worry about overflow here...
				 */
 vax_ovfl_check:
				word0(&rv) -= P*Exp_msk1;
				/* rv = */ rounded_product(dval(&rv), tens[e]);
				if ((word0(&rv) & Exp_mask)
				 > Exp_msk1*(DBL_MAX_EXP+Bias-1-P))
					goto ovfl;
				word0(&rv) += P*Exp_msk1;
#else
				/* rv = */ rounded_product(dval(&rv), tens[e]);
#endif
				goto ret;
				}
			}
#ifndef Inaccurate_Divide
		else if (e >= -Ten_pmax) {
#ifdef SET_INEXACT
				bc.inexact = 0;
				oldinexact = 1;
#endif
#ifdef Honor_FLT_ROUNDS
			/* round correctly FLT_ROUNDS = 2 or 3 */
			if (sign) {
				rv.d = -rv.d;
				sign = 0;
				}
#endif
			/* rv = */ rounded_quotient(dval(&rv), tens[-e]);
			goto ret;
			}
#endif
#endif /* ROUND_BIASED_without_Round_Up */
		}
#ifdef USE_BF96
	k = nd < 19 ? nd : 19;
#endif
	e1 += nd - k;	/* scale factor = 10^e1 */

#ifdef IEEE_Arith
#ifdef SET_INEXACT
	bc.inexact = 1;
#ifndef USE_BF96
	if (k <= DBL_DIG)
#endif
		oldinexact = get_inexact();
#endif
#ifdef Honor_FLT_ROUNDS
	if (bc.rounding >= 2) {
		if (sign)
			bc.rounding = bc.rounding == 2 ? 0 : 2;
		else
			if (bc.rounding != 2)
				bc.rounding = 0;
		}
#endif
#endif /*IEEE_Arith*/

#ifdef USE_BF96 /*{*/
	Debug(++__dtoa_stats[0]);
	i = e1 + 342;
	if (i < 0)
		goto undfl;
	if (i > 650)
		goto ovfl;
	p10 = &pten[i];
	brv = yz;
	/* shift brv left, with i =  number of bits shifted */
	i = 0;
	if (!(brv & 0xffffffff00000000ull)) {
		i = 32;
		brv <<= 32;
		}
	if (!(brv & 0xffff000000000000ull)) {
		i += 16;
		brv <<= 16;
		}
	if (!(brv & 0xff00000000000000ull)) {
		i += 8;
		brv <<= 8;
		}
	if (!(brv & 0xf000000000000000ull)) {
		i += 4;
		brv <<= 4;
		}
	if (!(brv & 0xc000000000000000ull)) {
		i += 2;
		brv <<= 2;
		}
	if (!(brv & 0x8000000000000000ull)) {
		i += 1;
		brv <<= 1;
		}
	erv = (64 + 0x3fe) + p10->e - i;
	if (erv <= 0 && nd > 19)
		goto many_digits; /* denormal: may need to look at all digits */
	bhi = brv >> 32;
	blo = brv & 0xffffffffull;
	/* Unsigned 32-bit ints lie in [0,2^32-1] and */
	/* unsigned 64-bit ints lie in [0, 2^64-1].  The product of two unsigned */
	/* 32-bit ints is <= 2^64 - 2*2^32-1 + 1 = 2^64 - 1 - 2*(2^32 - 1), so */
	/* we can add two unsigned 32-bit ints to the product of two such ints, */
	/* and 64 bits suffice to contain the result. */
	t01 = bhi * p10->b1;
	t10 = blo * p10->b0 + (t01 & 0xffffffffull);
	t00 = bhi * p10->b0 + (t01 >> 32) + (t10 >> 32);
	if (t00 & 0x8000000000000000ull) {
		if ((t00 & 0x3ff) && (~t00 & 0x3fe)) { /* unambiguous result? */
			if (nd > 19 && ((t00 + (1<<i) + 2) & 0x400) ^ (t00 & 0x400))
				goto many_digits;
			if (erv <= 0)
				goto denormal;
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround;
			  case 2: goto roundup;
			  }
#endif
			if (t00 & 0x400 && t00 & 0xbff)
				goto roundup;
			goto noround;
			}
		}
	else {
		if ((t00 & 0x1ff) && (~t00 & 0x1fe)) { /* unambiguous result? */
			if (nd > 19 && ((t00 + (1<<i) + 2) & 0x200) ^ (t00 & 0x200))
				goto many_digits;
			if (erv <= 1)
				goto denormal1;
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround1;
			  case 2: goto roundup1;
			  }
#endif
			if (t00 & 0x200)
				goto roundup1;
			goto noround1;
			}
		}
	/* 3 multiplies did not suffice; try a 96-bit approximation */
	Debug(++__dtoa_stats[1]);
	t02 = bhi * p10->b2;
	t11 = blo * p10->b1 + (t02 & 0xffffffffull);
	bexact = 1;
	if (e1 < 0 || e1 > 41 || (t10 | t11) & 0xffffffffull || nd > 19)
		bexact = 0;
	tlo = (t10 & 0xffffffffull) + (t02 >> 32) + (t11 >> 32);
	if (!bexact && (tlo + 0x10) >> 32 > tlo >> 32)
		goto many_digits;
	t00 += tlo >> 32;
	if (t00 & 0x8000000000000000ull) {
		if (erv <= 0) { /* denormal result */
			if (nd >= 20 || !((tlo & 0xfffffff0) | (t00 & 0x3ff)))
				goto many_digits;
 denormal:
			if (erv <= -52) {
#ifdef Honor_FLT_ROUNDS
				switch(bc.rounding) {
				  case 0: goto undfl;
				  case 2: goto tiniest;
				  }
#endif
				if (erv < -52 || !(t00 & 0x7fffffffffffffffull))
					goto undfl;
				goto tiniest;
				}
			tg = 1ull << (11 - erv);
			t00 &= ~(tg - 1); /* clear low bits */
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround_den;
			  case 2: goto roundup_den;
			  }
#endif
			if (t00 & tg) {
#ifdef Honor_FLT_ROUNDS
 roundup_den:
#endif
				t00 += tg << 1;
				if (!(t00 & 0x8000000000000000ull)) {
					if (++erv > 0)
						goto smallest_normal;
					t00 = 0x8000000000000000ull;
					}
				}
#ifdef Honor_FLT_ROUNDS
 noround_den:
#endif
			LLval(&rv) = t00 >> (12 - erv);
			Set_errno(ERANGE);
			goto ret;
			}
		if (bexact) {
#ifdef SET_INEXACT
			if (!(t00 & 0x7ff) && !(tlo & 0xffffffffull)) {
				bc.inexact = 0;
				goto noround;
				}
#endif
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 2:
				if (t00 & 0x7ff)
					goto roundup;
			  case 0: goto noround;
			  }
#endif
			if (t00 & 0x400 && (tlo & 0xffffffff) | (t00 & 0xbff))
				goto roundup;
			goto noround;
			}
		if ((tlo & 0xfffffff0) | (t00 & 0x3ff)
		 && (nd <= 19 ||  ((t00 + (1ull << i)) & 0xfffffffffffffc00ull)
				== (t00 & 0xfffffffffffffc00ull))) {
			/* Unambiguous result. */
			/* If nd > 19, then incrementing the 19th digit */
			/* does not affect rv. */
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround;
			  case 2: goto roundup;
			  }
#endif
			if (t00 & 0x400) { /* round up */
 roundup:
				t00 += 0x800;
				if (!(t00 & 0x8000000000000000ull)) {
					/* rounded up to a power of 2 */
					if (erv >= 0x7fe)
						goto ovfl;
					terv = erv + 1;
					LLval(&rv) = terv << 52;
					goto ret;
					}
				}
 noround:
			if (erv >= 0x7ff)
				goto ovfl;
			terv = erv;
			LLval(&rv) = (terv << 52) | ((t00 & 0x7ffffffffffff800ull) >> 11);
			goto ret;
			}
		}
	else {
		if (erv <= 1) { /* denormal result */
			if (nd >= 20 || !((tlo & 0xfffffff0) | (t00 & 0x1ff)))
				goto many_digits;
 denormal1:
			if (erv <= -51) {
#ifdef Honor_FLT_ROUNDS
				switch(bc.rounding) {
				  case 0: goto undfl;
				  case 2: goto tiniest;
				  }
#endif
				if (erv < -51 || !(t00 & 0x3fffffffffffffffull))
					goto undfl;
 tiniest:
				LLval(&rv) = 1;
				Set_errno(ERANGE);
				goto ret;
				}
			tg = 1ull << (11 - erv);
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround1_den;
			  case 2: goto roundup1_den;
			  }
#endif
			if (t00 & tg) {
#ifdef Honor_FLT_ROUNDS
 roundup1_den:
#endif
				if (0x8000000000000000ull & (t00 += (tg<<1)) && erv == 1) {

 smallest_normal:
					LLval(&rv) = 0x0010000000000000ull;
					goto ret;
					}
				}
#ifdef Honor_FLT_ROUNDS
 noround1_den:
#endif
			if (erv <= -52)
				goto undfl;
			LLval(&rv) = t00 >> (12 - erv);
			Set_errno(ERANGE);
			goto ret;
			}
		if (bexact) {
#ifdef SET_INEXACT
			if (!(t00 & 0x3ff) && !(tlo & 0xffffffffull)) {
				bc.inexact = 0;
				goto noround1;
				}
#endif
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 2:
				if (t00 & 0x3ff)
					goto roundup1;
			  case 0: goto noround1;
			  }
#endif
			if (t00 & 0x200 && (t00 & 0x5ff || tlo))
				goto roundup1;
			goto noround1;
			}
		if ((tlo & 0xfffffff0) | (t00 & 0x1ff)
		 && (nd <= 19 ||  ((t00 + (1ull << i)) & 0x7ffffffffffffe00ull)
				== (t00 & 0x7ffffffffffffe00ull))) {
			/* Unambiguous result. */
#ifdef Honor_FLT_ROUNDS
			switch(bc.rounding) {
			  case 0: goto noround1;
			  case 2: goto roundup1;
			  }
#endif
			if (t00 & 0x200) { /* round up */
 roundup1:
				t00 += 0x400;
				if (!(t00 & 0x4000000000000000ull)) {
					/* rounded up to a power of 2 */
					if (erv >= 0x7ff)
						goto ovfl;
					terv = erv;
					LLval(&rv) = terv << 52;
					goto ret;
					}
				}
 noround1:
			if (erv >= 0x800)
				goto ovfl;
			terv = erv - 1;
			LLval(&rv) = (terv << 52) | ((t00 & 0x3ffffffffffffc00ull) >> 10);
			goto ret;
			}
		}
 many_digits:
	Debug(++__dtoa_stats[2]);
	if (nd > 17) {
		if (nd > 18) {
			yz /= 100;
			e1 += 2;
			}
		else {
			yz /= 10;
			e1 += 1;
			}
		y = yz / 100000000;
		}
	else if (nd > 9) {
		i = nd - 9;
		y = (yz >> i) / pfive[i-1];
		}
	else
		y = yz;
	dval(&rv) = yz;
#endif /*}*/

#ifdef IEEE_Arith
#ifdef Avoid_Underflow
	bc.scale = 0;
#endif
#endif /*IEEE_Arith*/

	/* Get starting approximation = rv * 10**e1 */

	if (e1 > 0) {
		if ((i = e1 & 15))
			dval(&rv) *= tens[i];
		if (e1 &= ~15) {
			if (e1 > DBL_MAX_10_EXP) {
 ovfl:
				/* Can't trust HUGE_VAL */
#ifdef IEEE_Arith
#ifdef Honor_FLT_ROUNDS
				switch(bc.rounding) {
				  case 0: /* toward 0 */
				  case 3: /* toward -infinity */
					word0(&rv) = Big0;
					word1(&rv) = Big1;
					break;
				  default:
					word0(&rv) = Exp_mask;
					word1(&rv) = 0;
				  }
#else /*Honor_FLT_ROUNDS*/
				word0(&rv) = Exp_mask;
				word1(&rv) = 0;
#endif /*Honor_FLT_ROUNDS*/
#ifdef SET_INEXACT
				/* set overflow bit */
				dval(&rv0) = 1e300;
				dval(&rv0) *= dval(&rv0);
#endif
#else /*IEEE_Arith*/
				word0(&rv) = Big0;
				word1(&rv) = Big1;
#endif /*IEEE_Arith*/
 range_err:
				if (bd0) {
					__Bfree(bb MTb);
					__Bfree(bd MTb);
					__Bfree(bs MTb);
					__Bfree(bd0 MTb);
					__Bfree(delta MTb);
					}
				Set_errno(ERANGE);
				goto ret;
				}
			e1 >>= 4;
			for(j = 0; e1 > 1; j++, e1 >>= 1)
				if (e1 & 1)
					dval(&rv) *= bigtens[j];
		/* The last multiplication could overflow. */
			word0(&rv) -= P*Exp_msk1;
			dval(&rv) *= bigtens[j];
			if ((z = word0(&rv) & Exp_mask)
			 > Exp_msk1*(DBL_MAX_EXP+Bias-P))
				goto ovfl;
			if (z > Exp_msk1*(DBL_MAX_EXP+Bias-1-P)) {
				/* set to largest number */
				/* (Can't trust DBL_MAX) */
				word0(&rv) = Big0;
				word1(&rv) = Big1;
				}
			else
				word0(&rv) += P*Exp_msk1;
			}
		}
	else if (e1 < 0) {
		e1 = -e1;
		if ((i = e1 & 15))
			dval(&rv) /= tens[i];
		if (e1 >>= 4) {
			if (e1 >= 1 << n_bigtens)
				goto undfl;
#ifdef Avoid_Underflow
			if (e1 & Scale_Bit)
				bc.scale = 2*P;
			for(j = 0; e1 > 0; j++, e1 >>= 1)
				if (e1 & 1)
					dval(&rv) *= tinytens[j];
			if (bc.scale && (j = 2*P + 1 - ((word0(&rv) & Exp_mask)
						>> Exp_shift)) > 0) {
				/* scaled rv is denormal; clear j low bits */
				if (j >= 32) {
					if (j > 54)
						goto undfl;
					word1(&rv) = 0;
					if (j >= 53)
					 word0(&rv) = (P+2)*Exp_msk1;
					else
					 word0(&rv) &= 0xffffffff << (j-32);
					}
				else
					word1(&rv) &= 0xffffffff << j;
				}
#else
			for(j = 0; e1 > 1; j++, e1 >>= 1)
				if (e1 & 1)
					dval(&rv) *= tinytens[j];
			/* The last multiplication could underflow. */
			dval(&rv0) = dval(&rv);
			dval(&rv) *= tinytens[j];
			if (!dval(&rv)) {
				dval(&rv) = 2.*dval(&rv0);
				dval(&rv) *= tinytens[j];
#endif
				if (!dval(&rv)) {
 undfl:
					dval(&rv) = 0.;
#ifdef Honor_FLT_ROUNDS
					if (bc.rounding == 2)
						word1(&rv) = 1;
#endif
					goto range_err;
					}
#ifndef Avoid_Underflow
				word0(&rv) = Tiny0;
				word1(&rv) = Tiny1;
				/* The refinement below will clean
				 * this approximation up.
				 */
				}
#endif
			}
		}

	/* Now the hard part -- adjusting rv to the correct value.*/

	/* Put digits into bd: true value = bd * 10^e */

	bc.nd = nd - nz1;
#ifndef NO_STRTOD_BIGCOMP
	bc.nd0 = nd0;	/* Only needed if nd > __strtod_diglim, but done here */
			/* to silence an erroneous warning about bc.nd0 */
			/* possibly not being initialized. */
	if (nd > __strtod_diglim) {
		/* ASSERT(__strtod_diglim >= 18); 18 == one more than the */
		/* minimum number of decimal digits to distinguish double values */
		/* in IEEE arithmetic. */
		i = j = 18;
		if (i > nd0)
			j += bc.dplen;
		for(;;) {
			if (--j < bc.dp1 && j >= bc.dp0)
				j = bc.dp0 - 1;
			if (s0[j] != '0')
				break;
			--i;
			}
		e += nd - i;
		nd = i;
		if (nd0 > nd)
			nd0 = nd;
		if (nd < 9) { /* must recompute y */
			y = 0;
			for(i = 0; i < nd0; ++i)
				y = 10*y + s0[i] - '0';
			for(j = bc.dp1; i < nd; ++i)
				y = 10*y + s0[j++] - '0';
			}
		}
#endif
	bd0 = s2b(s0, nd0, nd, y, bc.dplen MTb);

	for(;;) {
		bd = __Balloc(bd0->k MTb);
		Bcopy(bd, bd0);
		bb = __d2b(&rv, &bbe, &bbbits MTb);	/* rv = bb * 2^bbe */
		bs = __i2b(1 MTb);

		if (e >= 0) {
			bb2 = bb5 = 0;
			bd2 = bd5 = e;
			}
		else {
			bb2 = bb5 = -e;
			bd2 = bd5 = 0;
			}
		if (bbe >= 0)
			bb2 += bbe;
		else
			bd2 -= bbe;
		bs2 = bb2;
#ifdef Honor_FLT_ROUNDS
		if (bc.rounding != 1)
			bs2++;
#endif
#ifdef Avoid_Underflow
		Lsb = LSB;
		Lsb1 = 0;
		j = bbe - bc.scale;
		i = j + bbbits - 1;	/* logb(rv) */
		j = P + 1 - bbbits;
		if (i < Emin) {	/* denormal */
			i = Emin - i;
			j -= i;
			if (i < 32)
				Lsb <<= i;
			else if (i < 52)
				Lsb1 = Lsb << (i-32);
			else
				Lsb1 = Exp_mask;
			}
#else /*Avoid_Underflow*/
#ifdef Sudden_Underflow
#ifdef IBM
		j = 1 + 4*P - 3 - bbbits + ((bbe + bbbits - 1) & 3);
#else
		j = P + 1 - bbbits;
#endif
#else /*Sudden_Underflow*/
		j = bbe;
		i = j + bbbits - 1;	/* logb(rv) */
		if (i < Emin)	/* denormal */
			j += P - Emin;
		else
			j = P + 1 - bbbits;
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
		bb2 += j;
		bd2 += j;
#ifdef Avoid_Underflow
		bd2 += bc.scale;
#endif
		i = bb2 < bd2 ? bb2 : bd2;
		if (i > bs2)
			i = bs2;
		if (i > 0) {
			bb2 -= i;
			bd2 -= i;
			bs2 -= i;
			}
		if (bb5 > 0) {
			bs = __pow5mult(bs, bb5 MTb);
			bb1 = __mult(bs, bb MTb);
			__Bfree(bb MTb);
			bb = bb1;
			}
		if (bb2 > 0)
			bb = __lshift(bb, bb2 MTb);
		if (bd5 > 0)
			bd = __pow5mult(bd, bd5 MTb);
		if (bd2 > 0)
			bd = __lshift(bd, bd2 MTb);
		if (bs2 > 0)
			bs = __lshift(bs, bs2 MTb);
		delta = __diff(bb, bd MTb);
		bc.dsign = delta->sign;
		delta->sign = 0;
		i = __cmp(delta, bs);
#ifndef NO_STRTOD_BIGCOMP /*{*/
		if (bc.nd > nd && i <= 0) {
			if (bc.dsign) {
				/* Must use bigcomp(). */
				req_bigcomp = 1;
				break;
				}
#ifdef Honor_FLT_ROUNDS
			if (bc.rounding != 1) {
				if (i < 0) {
					req_bigcomp = 1;
					break;
					}
				}
			else
#endif
				i = -1;	/* Discarded digits make delta smaller. */
			}
#endif /*}*/
#ifdef Honor_FLT_ROUNDS /*{*/
		if (bc.rounding != 1) {
			if (i < 0) {
				/* Error is less than an ulp */
				if (!delta->x[0] && delta->wds <= 1) {
					/* exact */
#ifdef SET_INEXACT
					bc.inexact = 0;
#endif
					break;
					}
				if (bc.rounding) {
					if (bc.dsign) {
						adj.d = 1.;
						goto apply_adj;
						}
					}
				else if (!bc.dsign) {
					adj.d = -1.;
					if (!word1(&rv)
					 && !(word0(&rv) & Frac_mask)) {
						y = word0(&rv) & Exp_mask;
#ifdef Avoid_Underflow
						if (!bc.scale || y > 2*P*Exp_msk1)
#else
						if (y)
#endif
						  {
						  delta = __lshift(delta,Log2P MTb);
						  if (__cmp(delta, bs) <= 0)
							adj.d = -0.5;
						  }
						}
 apply_adj:
#ifdef Avoid_Underflow /*{*/
					if (bc.scale && (y = word0(&rv) & Exp_mask)
						<= 2*P*Exp_msk1)
					  word0(&adj) += (2*P+1)*Exp_msk1 - y;
#else
#ifdef Sudden_Underflow
					if ((word0(&rv) & Exp_mask) <=
							P*Exp_msk1) {
						word0(&rv) += P*Exp_msk1;
						dval(&rv) += adj.d*ulp(dval(&rv));
						word0(&rv) -= P*Exp_msk1;
						}
					else
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow}*/
					dval(&rv) += adj.d*ulp(&rv);
					}
				break;
				}
			adj.d = ratio(delta, bs);
			if (adj.d < 1.)
				adj.d = 1.;
			if (adj.d <= 0x7ffffffe) {
				/* adj = rounding ? ceil(adj) : floor(adj); */
				y = adj.d;
				if (y != adj.d) {
					if (!((bc.rounding>>1) ^ bc.dsign))
						y++;
					adj.d = y;
					}
				}
#ifdef Avoid_Underflow /*{*/
			if (bc.scale && (y = word0(&rv) & Exp_mask) <= 2*P*Exp_msk1)
				word0(&adj) += (2*P+1)*Exp_msk1 - y;
#else
#ifdef Sudden_Underflow
			if ((word0(&rv) & Exp_mask) <= P*Exp_msk1) {
				word0(&rv) += P*Exp_msk1;
				adj.d *= ulp(dval(&rv));
				if (bc.dsign)
					dval(&rv) += adj.d;
				else
					dval(&rv) -= adj.d;
				word0(&rv) -= P*Exp_msk1;
				goto cont;
				}
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow}*/
			adj.d *= ulp(&rv);
			if (bc.dsign) {
				if (word0(&rv) == Big0 && word1(&rv) == Big1)
					goto ovfl;
				dval(&rv) += adj.d;
				}
			else
				dval(&rv) -= adj.d;
			goto cont;
			}
#endif /*}Honor_FLT_ROUNDS*/

		if (i < 0) {
			/* Error is less than half an ulp -- check for
			 * special case of mantissa a power of two.
			 */
			if (bc.dsign || word1(&rv) || word0(&rv) & Bndry_mask
#ifdef IEEE_Arith /*{*/
#ifdef Avoid_Underflow
			 || (word0(&rv) & Exp_mask) <= (2*P+1)*Exp_msk1
#else
			 || (word0(&rv) & Exp_mask) <= Exp_msk1
#endif
#endif /*}*/
				) {
#ifdef SET_INEXACT
				if (!delta->x[0] && delta->wds <= 1)
					bc.inexact = 0;
#endif
				break;
				}
			if (!delta->x[0] && delta->wds <= 1) {
				/* exact result */
#ifdef SET_INEXACT
				bc.inexact = 0;
#endif
				break;
				}
			delta = __lshift(delta,Log2P MTb);
			if (__cmp(delta, bs) > 0)
				goto drop_down;
			break;
			}
		if (i == 0) {
			/* exactly half-way between */
			if (bc.dsign) {
				if ((word0(&rv) & Bndry_mask1) == Bndry_mask1
				 &&  word1(&rv) == (
#ifdef Avoid_Underflow
			(bc.scale && (y = word0(&rv) & Exp_mask) <= 2*P*Exp_msk1)
		? (0xffffffff & (0xffffffff << (2*P+1-(y>>Exp_shift)))) :
#endif
						   0xffffffff)) {
					/*boundary case -- increment exponent*/
					if (word0(&rv) == Big0 && word1(&rv) == Big1)
						goto ovfl;
					word0(&rv) = (word0(&rv) & Exp_mask)
						+ Exp_msk1
#ifdef IBM
						| Exp_msk1 >> 4
#endif
						;
					word1(&rv) = 0;
#ifdef Avoid_Underflow
					bc.dsign = 0;
#endif
					break;
					}
				}
			else if (!(word0(&rv) & Bndry_mask) && !word1(&rv)) {
 drop_down:
				/* boundary case -- decrement exponent */
#ifdef Sudden_Underflow /*{{*/
				L = word0(&rv) & Exp_mask;
#ifdef IBM
				if (L <  Exp_msk1)
#else
#ifdef Avoid_Underflow
				if (L <= (bc.scale ? (2*P+1)*Exp_msk1 : Exp_msk1))
#else
				if (L <= Exp_msk1)
#endif /*Avoid_Underflow*/
#endif /*IBM*/
					{
					if (bc.nd >nd) {
						bc.uflchk = 1;
						break;
						}
					goto undfl;
					}
				L -= Exp_msk1;
#else /*Sudden_Underflow}{*/
#ifdef Avoid_Underflow
				if (bc.scale) {
					L = word0(&rv) & Exp_mask;
					if (L <= (2*P+1)*Exp_msk1) {
						if (L > (P+2)*Exp_msk1)
							/* round even ==> */
							/* accept rv */
							break;
						/* rv = smallest denormal */
						if (bc.nd >nd) {
							bc.uflchk = 1;
							break;
							}
						goto undfl;
						}
					}
#endif /*Avoid_Underflow*/
				L = (word0(&rv) & Exp_mask) - Exp_msk1;
#endif /*Sudden_Underflow}}*/
				word0(&rv) = L | Bndry_mask1;
				word1(&rv) = 0xffffffff;
#ifdef IBM
				goto cont;
#else
#ifndef NO_STRTOD_BIGCOMP
				if (bc.nd > nd)
					goto cont;
#endif
				break;
#endif
				}
#ifndef ROUND_BIASED
#ifdef Avoid_Underflow
			if (Lsb1) {
				if (!(word0(&rv) & Lsb1))
					break;
				}
			else if (!(word1(&rv) & Lsb))
				break;
#else
			if (!(word1(&rv) & LSB))
				break;
#endif
#endif
			if (bc.dsign)
#ifdef Avoid_Underflow
				dval(&rv) += sulp(&rv, &bc);
#else
				dval(&rv) += ulp(&rv);
#endif
#ifndef ROUND_BIASED
			else {
#ifdef Avoid_Underflow
				dval(&rv) -= sulp(&rv, &bc);
#else
				dval(&rv) -= ulp(&rv);
#endif
#ifndef Sudden_Underflow
				if (!dval(&rv)) {
					if (bc.nd >nd) {
						bc.uflchk = 1;
						break;
						}
					goto undfl;
					}
#endif
				}
#ifdef Avoid_Underflow
			bc.dsign = 1 - bc.dsign;
#endif
#endif
			break;
			}
		if ((aadj = ratio(delta, bs)) <= 2.) {
			if (bc.dsign)
				aadj = aadj1 = 1.;
			else if (word1(&rv) || word0(&rv) & Bndry_mask) {
#ifndef Sudden_Underflow
				if (word1(&rv) == Tiny1 && !word0(&rv)) {
					if (bc.nd >nd) {
						bc.uflchk = 1;
						break;
						}
					goto undfl;
					}
#endif
				aadj = 1.;
				aadj1 = -1.;
				}
			else {
				/* special case -- power of FLT_RADIX to be */
				/* rounded down... */

				if (aadj < 2./FLT_RADIX)
					aadj = 1./FLT_RADIX;
				else
					aadj *= 0.5;
				aadj1 = -aadj;
				}
			}
		else {
			aadj *= 0.5;
			aadj1 = bc.dsign ? aadj : -aadj;
#ifdef Check_FLT_ROUNDS
			switch(bc.rounding) {
				case 2: /* towards +infinity */
					aadj1 -= 0.5;
					break;
				case 0: /* towards 0 */
				case 3: /* towards -infinity */
					aadj1 += 0.5;
				}
#else
			if (Flt_Rounds == 0)
				aadj1 += 0.5;
#endif /*Check_FLT_ROUNDS*/
			}
		y = word0(&rv) & Exp_mask;

		/* Check for overflow */

		if (y == Exp_msk1*(DBL_MAX_EXP+Bias-1)) {
			dval(&rv0) = dval(&rv);
			word0(&rv) -= P*Exp_msk1;
			adj.d = aadj1 * ulp(&rv);
			dval(&rv) += adj.d;
			if ((word0(&rv) & Exp_mask) >=
					Exp_msk1*(DBL_MAX_EXP+Bias-P)) {
				if (word0(&rv0) == Big0 && word1(&rv0) == Big1)
					goto ovfl;
				word0(&rv) = Big0;
				word1(&rv) = Big1;
				goto cont;
				}
			else
				word0(&rv) += P*Exp_msk1;
			}
		else {
#ifdef Avoid_Underflow
			if (bc.scale && y <= 2*P*Exp_msk1) {
				if (aadj <= 0x7fffffff) {
					if ((z = aadj) <= 0)
						z = 1;
					aadj = z;
					aadj1 = bc.dsign ? aadj : -aadj;
					}
				dval(&aadj2) = aadj1;
				word0(&aadj2) += (2*P+1)*Exp_msk1 - y;
				aadj1 = dval(&aadj2);
				adj.d = aadj1 * ulp(&rv);
				dval(&rv) += adj.d;
				if (rv.d == 0.)
#ifdef NO_STRTOD_BIGCOMP
					goto undfl;
#else
					{
					req_bigcomp = 1;
					break;
					}
#endif
				}
			else {
				adj.d = aadj1 * ulp(&rv);
				dval(&rv) += adj.d;
				}
#else
#ifdef Sudden_Underflow
			if ((word0(&rv) & Exp_mask) <= P*Exp_msk1) {
				dval(&rv0) = dval(&rv);
				word0(&rv) += P*Exp_msk1;
				adj.d = aadj1 * ulp(&rv);
				dval(&rv) += adj.d;
#ifdef IBM
				if ((word0(&rv) & Exp_mask) <  P*Exp_msk1)
#else
				if ((word0(&rv) & Exp_mask) <= P*Exp_msk1)
#endif
					{
					if (word0(&rv0) == Tiny0
					 && word1(&rv0) == Tiny1) {
						if (bc.nd >nd) {
							bc.uflchk = 1;
							break;
							}
						goto undfl;
						}
					word0(&rv) = Tiny0;
					word1(&rv) = Tiny1;
					goto cont;
					}
				else
					word0(&rv) -= P*Exp_msk1;
				}
			else {
				adj.d = aadj1 * ulp(&rv);
				dval(&rv) += adj.d;
				}
#else /*Sudden_Underflow*/
			/* Compute adj so that the IEEE rounding rules will
			 * correctly round rv + adj in some half-way cases.
			 * If rv * ulp(rv) is denormalized (i.e.,
			 * y <= (P-1)*Exp_msk1), we must adjust aadj to avoid
			 * trouble from bits lost to denormalization;
			 * example: 1.2e-307 .
			 */
			if (y <= (P-1)*Exp_msk1 && aadj > 1.) {
				aadj1 = (double)(int)(aadj + 0.5);
				if (!bc.dsign)
					aadj1 = -aadj1;
				}
			adj.d = aadj1 * ulp(&rv);
			dval(&rv) += adj.d;
#endif /*Sudden_Underflow*/
#endif /*Avoid_Underflow*/
			}
		z = word0(&rv) & Exp_mask;
#ifndef SET_INEXACT
		if (bc.nd == nd) {
#ifdef Avoid_Underflow
		if (!bc.scale)
#endif
		if (y == z) {
			/* Can we stop now? */
			L = (Long)aadj;
			aadj -= L;
			/* The tolerances below are conservative. */
			if (bc.dsign || word1(&rv) || word0(&rv) & Bndry_mask) {
				if (aadj < .4999999 || aadj > .5000001)
					break;
				}
			else if (aadj < .4999999/FLT_RADIX)
				break;
			}
		}
#endif
 cont:
		__Bfree(bb MTb);
		__Bfree(bd MTb);
		__Bfree(bs MTb);
		__Bfree(delta MTb);
		}
	__Bfree(bb MTb);
	__Bfree(bd MTb);
	__Bfree(bs MTb);
	__Bfree(bd0 MTb);
	__Bfree(delta MTb);
#ifndef NO_STRTOD_BIGCOMP
	if (req_bigcomp) {
		bd0 = 0;
		bc.e0 += nz1;
		bigcomp(&rv, s0, &bc MTb);
		y = word0(&rv) & Exp_mask;
		if (y == Exp_mask)
			goto ovfl;
		if (y == 0 && rv.d == 0.)
			goto undfl;
		}
#endif
#ifdef Avoid_Underflow
	if (bc.scale) {
		word0(&rv0) = Exp_1 - 2*P*Exp_msk1;
		word1(&rv0) = 0;
		dval(&rv) *= dval(&rv0);
#ifndef NO_ERRNO
		/* try to avoid the bug of testing an 8087 register value */
#ifdef IEEE_Arith
		if (!(word0(&rv) & Exp_mask))
#else
		if (word0(&rv) == 0 && word1(&rv) == 0)
#endif
			Set_errno(ERANGE);
#endif
		}
#endif /* Avoid_Underflow */
 ret:
#ifdef SET_INEXACT
	if (bc.inexact) {
		if (!(word0(&rv) & Exp_mask)) {
			/* set underflow and inexact bits */
			dval(&rv0) = 1e-300;
			dval(&rv0) *= dval(&rv0);
			}
		else if (!oldinexact) {
			word0(&rv0) = Exp_1 + (70 << Exp_shift);
			word1(&rv0) = 0;
			dval(&rv0) += 1.;
			}
		}
	else if (!oldinexact)
		clear_inexact();
#endif
	if (se)
		*se = (char *)s;
	return sign ? -dval(&rv) : dval(&rv);
	}
