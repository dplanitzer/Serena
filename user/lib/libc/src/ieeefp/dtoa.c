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

#ifndef MULTIPLE_THREADS
 static char *dtoa_result;
#endif

 static char *
rv_alloc(int i MTd)
{
	int j, k, *r;

	j = sizeof(ULong);
	for(k = 0;
		sizeof(Bigint) - sizeof(ULong) - sizeof(int) + j <= i;
		j <<= 1)
			k++;
	r = (int*)__Balloc(k MTa);
	*r = k;
	return
#ifndef MULTIPLE_THREADS
	dtoa_result =
#endif
		(char *)(r+1);
	}

 static char *
nrv_alloc(const char *s, char *s0, size_t s0len, char **rve, int n MTd)
{
	char *rv, *t;

	if (!s0)
		s0 = rv_alloc(n MTa);
	else if (s0len <= n) {
		rv = 0;
		t = rv + n;
		goto rve_chk;
		}
	t = rv = s0;
	while((*t = *s++))
		++t;
 rve_chk:
	if (rve)
		*rve = t;
	return rv;
	}

/* freedtoa(s) must be used to free values s returned by dtoa
 * when MULTIPLE_THREADS is #defined.  It should be used in all cases,
 * but for consistency with earlier versions of dtoa, it is optional
 * when MULTIPLE_THREADS is not defined.
 */

 void
freedtoa(char *s)
{
#ifdef MULTIPLE_THREADS
	ThInfo *TI = 0;
#endif
	Bigint *b = (Bigint *)((int *)s - 1);
	b->maxwds = 1 << (b->k = *(int*)b);
	__Bfree(b MTb);
#ifndef MULTIPLE_THREADS
	if (s == dtoa_result)
		dtoa_result = 0;
#endif
	}

/* dtoa for IEEE arithmetic (dmg): convert double to ASCII string.
 *
 * Inspired by "How to Print Floating-Point Numbers Accurately" by
 * Guy L. Steele, Jr. and Jon L. White [Proc. ACM SIGPLAN '90, pp. 112-126].
 *
 * Modifications:
 *	1. Rather than iterating, we use a simple numeric overestimate
 *	   to determine k = floor(log10(d)).  We scale relevant
 *	   quantities using O(log2(k)) rather than O(k) multiplications.
 *	2. For some modes > 2 (corresponding to ecvt and fcvt), we don't
 *	   try to generate digits strictly left to right.  Instead, we
 *	   compute with fewer bits and propagate the carry if necessary
 *	   when rounding the final digit up.  This is often faster.
 *	3. Under the assumption that input will be rounded nearest,
 *	   mode 0 renders 1e23 as 1e23 rather than 9.999999999999999e22.
 *	   That is, we allow equality in stopping tests when the
 *	   round-nearest rule will give the same floating-point value
 *	   as would satisfaction of the stopping test with strict
 *	   inequality.
 *	4. We remove common factors of powers of 2 from relevant
 *	   quantities.
 *	5. When converting floating-point integers less than 1e16,
 *	   we use floating-point arithmetic rather than resorting
 *	   to multiple-precision integers.
 *	6. When asked to produce fewer than 15 digits, we first try
 *	   to get by with floating-point arithmetic; we resort to
 *	   multiple-precision integer arithmetic only if we cannot
 *	   guarantee that the floating-point calculation has given
 *	   the correctly rounded result.  For k requested digits and
 *	   "uniformly" distributed input, the probability is
 *	   something like 10^(k-15) that we must resort to the Long
 *	   calculation.
 */

 char *
__dtoa_r(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve, char *buf, size_t blen)
{
 /*	Arguments ndigits, decpt, sign are similar to those
	of ecvt and fcvt; trailing zeros are suppressed from
	the returned string.  If not null, *rve is set to point
	to the end of the return value.  If d is +-Infinity or NaN,
	then *decpt is set to 9999.

	mode:
		0 ==> shortest string that yields d when read in
			and rounded to nearest.
		1 ==> like 0, but with Steele & White stopping rule;
			e.g. with IEEE P754 arithmetic , mode 0 gives
			1e23 whereas mode 1 gives 9.999999999999999e22.
		2 ==> max(1,ndigits) significant digits.  This gives a
			return value similar to that of ecvt, except
			that trailing zeros are suppressed.
		3 ==> through ndigits past the decimal point.  This
			gives a return value similar to that from fcvt,
			except that trailing zeros are suppressed, and
			ndigits can be negative.
		4,5 ==> similar to 2 and 3, respectively, but (in
			round-nearest mode) with the tests of mode 0 to
			possibly return a shorter string that rounds to d.
			With IEEE arithmetic and compilation with
			-DHonor_FLT_ROUNDS, modes 4 and 5 behave the same
			as modes 2 and 3 when FLT_ROUNDS != 1.
		6-9 ==> Debugging modes similar to mode - 4:  don't try
			fast floating-point estimate (if applicable).

		Values of mode other than 0-9 are treated as mode 0.

	When not NULL, buf is an output buffer of length blen, which must
	be large enough to accommodate suppressed trailing zeros and a trailing
	null byte.  If blen is too small, rv = NULL is returned, in which case
	if rve is not NULL, a subsequent call with blen >= (*rve - rv) + 1
	should succeed in returning buf.

	When buf is NULL, sufficient space is allocated for the return value,
	which, when done using, the caller should pass to freedtoa().

	USE_BF is automatically defined when neither NO_LONG_LONG nor NO_BF96
	is defined.
	*/

#ifdef MULTIPLE_THREADS
	ThInfo *TI = 0;
#endif
	int bbits, b2, b5, be, dig, i, ilim, ilim1,
		j, j1, k, leftright, m2, m5, s2, s5, spec_case;
#if !defined(Sudden_Underflow) || defined(USE_BF96)
	int denorm;
#endif
	Bigint *b, *b1, *delta, *mlo, *mhi, *S;
	U u;
	char *s;
#ifdef SET_INEXACT
	int inexact, oldinexact;
#endif
#ifdef USE_BF96 /*{{*/
	BF96 *p10;
	ULLong dbhi, dbits, dblo, den, hb, rb, rblo, res, res0, res3, reslo, sres,
		sulp, tv0, tv1, tv2, tv3, ulp, ulplo, ulpmask, ures, ureslo, zb;
	int eulp, k1, n2, ulpadj, ulpshift;
#else /*}{*/
#ifndef Sudden_Underflow
	ULong x;
#endif
	Long L;
	U d2, eps;
	double ds;
	int ieps, ilim0, k0, k_check, try_quick;
#ifndef No_leftright
#ifdef IEEE_Arith
	U eps1;
#endif
#endif
#endif /*}}*/
#ifdef Honor_FLT_ROUNDS /*{*/
	int Rounding;
#ifdef Trust_FLT_ROUNDS /*{{ only define this if FLT_ROUNDS really works! */
	Rounding = Flt_Rounds;
#else /*}{*/
	Rounding = 1;
	switch(fegetround()) {
	  case FE_TOWARDZERO:	Rounding = 0; break;
	  case FE_UPWARD:	Rounding = 2; break;
	  case FE_DOWNWARD:	Rounding = 3;
	  }
#endif /*}}*/
#endif /*}*/

	u.d = dd;
	if (word0(&u) & Sign_bit) {
		/* set sign for everything, including 0's and NaNs */
		*sign = 1;
		word0(&u) &= ~Sign_bit;	/* clear sign bit */
		}
	else
		*sign = 0;

#if defined(IEEE_Arith) + defined(VAX)
#ifdef IEEE_Arith
	if ((word0(&u) & Exp_mask) == Exp_mask)
#else
	if (word0(&u)  == 0x8000)
#endif
		{
		/* Infinity or NaN */
		*decpt = 9999;
#ifdef IEEE_Arith
		if (!word1(&u) && !(word0(&u) & 0xfffff))
			return nrv_alloc("Infinity", buf, blen, rve, 8 MTb);
#endif
		return nrv_alloc("NaN", buf, blen, rve, 3 MTb);
		}
#endif
#ifdef IBM
	dval(&u) += 0; /* normalize */
#endif
	if (!dval(&u)) {
		*decpt = 1;
		return nrv_alloc("0", buf, blen, rve, 1 MTb);
		}

#ifdef SET_INEXACT
#ifndef USE_BF96
	try_quick =
#endif
	oldinexact = get_inexact();
	inexact = 1;
#endif
#ifdef Honor_FLT_ROUNDS
	if (Rounding >= 2) {
		if (*sign)
			Rounding = Rounding == 2 ? 0 : 2;
		else
			if (Rounding != 2)
				Rounding = 0;
		}
#endif
#ifdef USE_BF96 /*{{*/
	dbits = (u.LL & 0xfffffffffffffull) << 11;	/* fraction bits */
	if ((be = u.LL >> 52)) /* biased exponent; nonzero ==> normal */ {
		dbits |= 0x8000000000000000ull;
		denorm = ulpadj = 0;
		}
	else {
		denorm = 1;
		ulpadj = be + 1;
		dbits <<= 1;
		if (!(dbits & 0xffffffff00000000ull)) {
			dbits <<= 32;
			be -= 32;
			}
		if (!(dbits & 0xffff000000000000ull)) {
			dbits <<= 16;
			be -= 16;
			}
		if (!(dbits & 0xff00000000000000ull)) {
			dbits <<= 8;
			be -= 8;
			}
		if (!(dbits & 0xf000000000000000ull)) {
			dbits <<= 4;
			be -= 4;
			}
		if (!(dbits & 0xc000000000000000ull)) {
			dbits <<= 2;
			be -= 2;
			}
		if (!(dbits & 0x8000000000000000ull)) {
			dbits <<= 1;
			be -= 1;
			}
		assert(be >= -51);
		ulpadj -= be;
		}
	j = Lhint[be + 51];
	p10 = &pten[j];
	dbhi = dbits >> 32;
	dblo = dbits & 0xffffffffull;
	i = be - 0x3fe;
	if (i < p10->e
	|| (i == p10->e && (dbhi < p10->b0 || (dbhi == p10->b0 && dblo < p10->b1))))
		--j;
	k = j - 342;

	/* now 10^k <= dd < 10^(k+1) */

#else /*}{*/

	b = __d2b(&u, &be, &bbits MTb);
#ifdef Sudden_Underflow
	i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1));
#else
	if ((i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1)))) {
#endif
		dval(&d2) = dval(&u);
		word0(&d2) &= Frac_mask1;
		word0(&d2) |= Exp_11;
#ifdef IBM
		if (j = 11 - __hi0bits(word0(&d2) & Frac_mask))
			dval(&d2) /= 1 << j;
#endif

		/* log(x)	~=~ log(1.5) + (x-1.5)/1.5
		 * log10(x)	 =  log(x) / log(10)
		 *		~=~ log(1.5)/log(10) + (x-1.5)/(1.5*log(10))
		 * log10(d) = (i-Bias)*log(2)/log(10) + log10(d2)
		 *
		 * This suggests computing an approximation k to log10(d) by
		 *
		 * k = (i - Bias)*0.301029995663981
		 *	+ ( (d2-1.5)*0.289529654602168 + 0.176091259055681 );
		 *
		 * We want k to be too large rather than too small.
		 * The error in the first-order Taylor series approximation
		 * is in our favor, so we just round up the constant enough
		 * to compensate for any error in the multiplication of
		 * (i - Bias) by 0.301029995663981; since |i - Bias| <= 1077,
		 * and 1077 * 0.30103 * 2^-52 ~=~ 7.2e-14,
		 * adding 1e-13 to the constant term more than suffices.
		 * Hence we adjust the constant term to 0.1760912590558.
		 * (We could get a more accurate k by invoking log10,
		 *  but this is probably not worthwhile.)
		 */

		i -= Bias;
#ifdef IBM
		i <<= 2;
		i += j;
#endif
#ifndef Sudden_Underflow
		denorm = 0;
		}
	else {
		/* d is denormalized */

		i = bbits + be + (Bias + (P-1) - 1);
		x = i > 32  ? word0(&u) << (64 - i) | word1(&u) >> (i - 32)
			    : word1(&u) << (32 - i);
		dval(&d2) = x;
		word0(&d2) -= 31*Exp_msk1; /* adjust exponent */
		i -= (Bias + (P-1) - 1) + 1;
		denorm = 1;
		}
#endif
	ds = (dval(&d2)-1.5)*0.289529654602168 + 0.1760912590558 + i*0.301029995663981;
	k = (int)ds;
	if (ds < 0. && ds != k)
		k--;	/* want k = floor(ds) */
	k_check = 1;
	if (k >= 0 && k <= Ten_pmax) {
		if (dval(&u) < tens[k])
			k--;
		k_check = 0;
		}
	j = bbits - i - 1;
	if (j >= 0) {
		b2 = 0;
		s2 = j;
		}
	else {
		b2 = -j;
		s2 = 0;
		}
	if (k >= 0) {
		b5 = 0;
		s5 = k;
		s2 += k;
		}
	else {
		b2 -= k;
		b5 = -k;
		s5 = 0;
		}
#endif /*}}*/
	if (mode < 0 || mode > 9)
		mode = 0;

#ifndef USE_BF96
#ifndef SET_INEXACT
#ifdef Check_FLT_ROUNDS
	try_quick = Rounding == 1;
#else
	try_quick = 1;
#endif
#endif /*SET_INEXACT*/
#endif /*USE_BF96*/

	if (mode > 5) {
		mode -= 4;
#ifndef USE_BF96
		try_quick = 0;
#endif
		}
	leftright = 1;
	ilim = ilim1 = -1;	/* Values for cases 0 and 1; done here to */
				/* silence erroneous "gcc -Wall" warning. */
	switch(mode) {
		case 0:
		case 1:
			i = 18;
			ndigits = 0;
			break;
		case 2:
			leftright = 0;
			/* no break */
		case 4:
			if (ndigits <= 0)
				ndigits = 1;
			ilim = ilim1 = i = ndigits;
			break;
		case 3:
			leftright = 0;
			/* no break */
		case 5:
			i = ndigits + k + 1;
			ilim = i;
			ilim1 = i - 1;
			if (i <= 0)
				i = 1;
		}
	if (!buf) {
		buf = rv_alloc(i MTb);
		blen = sizeof(Bigint) + ((1 << ((int*)buf)[-1]) - 1)*sizeof(ULong) - sizeof(int);
		}
	else if (blen <= i) {
		buf = 0;
		if (rve)
			*rve = buf + i;
		return buf;
		}
	s = buf;

	/* Check for special case that d is a normalized power of 2. */

	spec_case = 0;
	if (mode < 2 || (leftright
#ifdef Honor_FLT_ROUNDS
			&& Rounding == 1
#endif
				)) {
		if (!word1(&u) && !(word0(&u) & Bndry_mask)
#ifndef Sudden_Underflow
		 && word0(&u) & (Exp_mask & ~Exp_msk1)
#endif
				) {
			/* The special case */
			spec_case = 1;
			}
		}

#ifdef USE_BF96 /*{*/
	b = 0;
	if (ilim < 0 && (mode == 3 || mode == 5)) {
		S = mhi = 0;
		goto no_digits;
		}
	i = 1;
	j = 52 + 0x3ff - be;
	ulpshift = 0;
	ulplo = 0;
	/* Can we do an exact computation with 64-bit integer arithmetic? */
	if (k < 0) {
		if (k < -25)
			goto toobig;
		res = dbits >> 11;
		n2 = pfivebits[k1 = -(k + 1)] + 53;
		j1 = j;
		if (n2 > 61) {
			ulpshift = n2 - 61;
			if (res & (ulpmask = (1ull << ulpshift) - 1))
				goto toobig;
			j -= ulpshift;
			res >>= ulpshift;
			}
		/* Yes. */
		res *= ulp = pfive[k1];
		if (ulpshift) {
			ulplo = ulp;
			ulp >>= ulpshift;
			}
		j += k;
		if (ilim == 0) {
			S = mhi = 0;
			if (res > (5ull << j))
				goto one_digit;
			goto no_digits;
			}
		goto no_div;
		}
	if (ilim == 0 && j + k >= 0) {
		S = mhi = 0;
		if ((dbits >> 11) > (pfive[k-1] << j))
			goto one_digit;
		goto no_digits;
		}
	if (k <= dtoa_divmax && j + k >= 0) {
		/* Another "yes" case -- we will use exact integer arithmetic. */
 use_exact:
		Debug(++__dtoa_stats[3]);
		res = dbits >> 11;	/* residual */
		ulp = 1;
		if (k <= 0)
			goto no_div;
		j1 = j + k + 1;
		den = pfive[k-i] << (j1 - i);
		for(;;) {
			dig = res / den;
			*s++ = '0' + dig;
			if (!(res -= dig*den)) {
#ifdef SET_INEXACT
				inexact = 0;
				oldinexact = 1;
#endif
				goto retc;
				}
			if (ilim < 0) {
				ures = den - res;
				if (2*res <= ulp
				&& (spec_case ? 4*res <= ulp : (2*res < ulp || dig & 1)))
					goto ulp_reached;
				if (2*ures < ulp)
					goto Roundup;
				}
			else if (i == ilim) {
				switch(Rounding) {
				  case 0: goto retc;
				  case 2: goto Roundup;
				  }
				ures = 2*res;
				if (ures > den
				|| (ures == den && dig & 1)
				|| (spec_case && res <= ulp && 2*res >= ulp))
					goto Roundup;
				goto retc;
				}
			if (j1 < ++i) {
				res *= 10;
				ulp *= 10;
				}
			else {
				if (i > k)
					break;
				den = pfive[k-i] << (j1 - i);
				}
			}
 no_div:
		for(;;) {
			dig = den = res >> j;
			*s++ = '0' + dig;
			if (!(res -= den << j)) {
#ifdef SET_INEXACT
				inexact = 0;
				oldinexact = 1;
#endif
				goto retc;
				}
			if (ilim < 0) {
				ures = (1ull << j) - res;
				if (2*res <= ulp
				&& (spec_case ? 4*res <= ulp : (2*res < ulp || dig & 1))) {
 ulp_reached:
					if (ures < res
					|| (ures == res && dig & 1)
					|| (dig == 9 && 2*ures <= ulp))
						goto Roundup;
					goto retc;
					}
				if (2*ures < ulp)
					goto Roundup;
				}
			--j;
			if (i == ilim) {
#ifdef Honor_FLT_ROUNDS
				switch(Rounding) {
				  case 0: goto retc;
				  case 2: goto Roundup;
				  }
#endif
				hb = 1ull << j;
				if (res & hb && (dig & 1 || res & (hb-1)))
					goto Roundup;
				if (spec_case && res <= ulp && 2*res >= ulp) {
 Roundup:
					while(*--s == '9')
						if (s == buf) {
							++k;
							*s++ = '1';
							goto ret1;
							}
					++*s++;
					goto ret1;
					}
				goto retc;
				}
			++i;
			res *= 5;
			if (ulpshift) {
				ulplo = 5*(ulplo & ulpmask);
				ulp = 5*ulp + (ulplo >> ulpshift);
				}
			else
				ulp *= 5;
			}
		}
 toobig:
	if (ilim > 28)
		goto Fast_failed1;
	/* Scale by 10^-k */
	p10 = &pten[342-k];
	tv0 = p10->b2 * dblo; /* rarely matters, but does, e.g., for 9.862818194192001e18 */
	tv1 = p10->b1 * dblo + (tv0 >> 32);
	tv2 = p10->b2 * dbhi + (tv1 & 0xffffffffull);
	tv3 = p10->b0 * dblo + (tv1>>32) + (tv2>>32);
	res3 = p10->b1 * dbhi + (tv3 & 0xffffffffull);
	res = p10->b0 * dbhi + (tv3>>32) + (res3>>32);
	be += p10->e - 0x3fe;
	eulp = j1 = be - 54 + ulpadj;
	if (!(res & 0x8000000000000000ull)) {
		--be;
		res3 <<= 1;
		res = (res << 1) | ((res3 & 0x100000000ull) >> 32);
		}
	res0 = res; /* save for Fast_failed */
#if !defined(SET_INEXACT) && !defined(NO_DTOA_64) /*{*/
	if (ilim > 19)
		goto Fast_failed;
	Debug(++__dtoa_stats[4]);
	assert(be >= 0 && be <= 4); /* be = 0 is rare, but possible, e.g., for 1e20 */
	res >>= 4 - be;
	ulp = p10->b0;	/* ulp */
	ulp = (ulp << 29) | (p10->b1 >> 3);
	/* scaled ulp = ulp * 2^(eulp - 60) */
	/* We maintain 61 bits of the scaled ulp. */
	if (ilim == 0) {
		if (!(res & 0x7fffffffffffffeull)
		 || !((~res) & 0x7fffffffffffffeull))
			goto Fast_failed1;
		S = mhi = 0;
		if (res >= 0x5000000000000000ull)
			goto one_digit;
		goto no_digits;
		}
	rb = 1;	/* upper bound on rounding error */
	for(;;++i) {
		dig = res >> 60;
		*s++ = '0' + dig;
		res &= 0xfffffffffffffffull;
		if (ilim < 0) {
			ures = 0x1000000000000000ull - res;
			if (eulp > 0) {
				assert(eulp <= 4);
				sulp = ulp << (eulp - 1);
				if (res <= ures) {
					if (res + rb > ures - rb)
						goto Fast_failed;
					if (res < sulp)
						goto retc;
					}
				else {
					if (res - rb <= ures + rb)
						goto Fast_failed;
					if (ures < sulp)
						goto Roundup;
					}
				}
			else {
				zb = -(1ull << (eulp + 63));
				if (!(zb & res)) {
					sres = res << (1 - eulp);
					if (sres < ulp && (!spec_case || 2*sres < ulp)) {
						if ((res+rb) << (1 - eulp) >= ulp)
							goto Fast_failed;
						if (ures < res) {
							if (ures + rb >= res - rb)
								goto Fast_failed;
							goto Roundup;
							}
						if (ures - rb < res + rb)
							goto Fast_failed;
						goto retc;
						}
					}
				if (!(zb & ures) && ures << -eulp < ulp) {
					if (ures << (1 - eulp) < ulp)
						goto  Roundup;
					goto Fast_failed;
					}
				}
			}
		else if (i == ilim) {
			ures = 0x1000000000000000ull - res;
			if (ures < res) {
				if (ures <= rb || res - rb <= ures + rb) {
					if (j + k >= 0 && k >= 0 && k <= 27)
						goto use_exact1;
					goto Fast_failed;
					}
#ifdef Honor_FLT_ROUNDS
				if (Rounding == 0)
					goto retc;
#endif
				goto Roundup;
				}
			if (res <= rb || ures - rb <= res + rb) {
				if (j + k >= 0 && k >= 0 && k <= 27) {
 use_exact1:
					s = buf;
					i = 1;
					goto use_exact;
					}
				goto Fast_failed;
				}
#ifdef Honor_FLT_ROUNDS
			if (Rounding == 2)
				goto Roundup;
#endif
			goto retc;
			}
		rb *= 10;
		if (rb >= 0x1000000000000000ull)
			goto Fast_failed;
		res *= 10;
		ulp *= 5;
		if (ulp & 0x8000000000000000ull) {
			eulp += 4;
			ulp >>= 3;
			}
		else {
			eulp += 3;
			ulp >>= 2;
			}
		}
#endif /*}*/
#ifndef NO_BF96
 Fast_failed:
#endif
	Debug(++__dtoa_stats[5]);
	s = buf;
	i = 4 - be;
	res = res0 >> i;
	reslo = 0xffffffffull & res3;
	if (i)
		reslo = (res0 << (64 - i)) >> 32 | (reslo >> i);
	rb = 0;
	rblo = 4; /* roundoff bound */
	ulp = p10->b0;	/* ulp */
	ulp = (ulp << 29) | (p10->b1 >> 3);
	eulp = j1;
	for(i = 1;;++i) {
		dig = res >> 60;
		*s++ = '0' + dig;
		res &= 0xfffffffffffffffull;
#ifdef SET_INEXACT
		if (!res && !reslo) {
			if (!(res3 & 0xffffffffull)) {
				inexact = 0;
				oldinexact = 1;
				}
			goto retc;
			}
#endif
		if (ilim < 0) {
			ures = 0x1000000000000000ull - res;
			ureslo = 0;
			if (reslo) {
				ureslo = 0x100000000ull - reslo;
				--ures;
				}
			if (eulp > 0) {
				assert(eulp <= 4);
				sulp = (ulp << (eulp - 1)) - rb;
				if (res <= ures) {
					if (res < sulp) {
						if (res+rb < ures-rb)
							goto retc;
						}
					}
				else if (ures < sulp) {
					if (res-rb > ures+rb)
						goto Roundup;
					}
				goto Fast_failed1;
				}
			else {
				zb = -(1ull << (eulp + 60));
				if (!(zb & (res + rb))) {
					sres = (res - rb) << (1 - eulp);
					if (sres < ulp && (!spec_case || 2*sres < ulp)) {
						sres = res << (1 - eulp);
						if ((j = eulp + 31) > 0)
							sres += (rblo + reslo) >> j;
						else
							sres += (rblo + reslo) << -j;
						if (sres + (rb << (1 - eulp)) >= ulp)
							goto Fast_failed1;
						if (sres >= ulp)
							goto more96;
						if (ures < res
						|| (ures == res && ureslo < reslo)) {
							if (ures + rb >= res - rb)
								goto Fast_failed1;
							goto Roundup;
							}
						if (ures - rb <= res + rb)
							goto Fast_failed1;
						goto retc;
						}
					}
				if (!(zb & ures) && (ures-rb) << (1 - eulp) < ulp) {
					if ((ures + rb) << (2 - eulp) < ulp)
						goto Roundup;
					goto Fast_failed1;
					}
				}
			}
		else if (i == ilim) {
			ures = 0x1000000000000000ull - res;
			sres = ureslo = 0;
			if (reslo) {
				ureslo = 0x100000000ull - reslo;
				--ures;
				sres = (reslo + rblo) >> 31;
				}
			sres += 2*rb;
			if (ures <= res) {
				if (ures <=sres || res - ures <= sres)
					goto Fast_failed1;
#ifdef Honor_FLT_ROUNDS
				if (Rounding == 0)
					goto retc;
#endif
				goto Roundup;
				}
			if (res <= sres || ures - res <= sres)
				goto Fast_failed1;
#ifdef Honor_FLT_ROUNDS
			if (Rounding == 2)
				goto Roundup;
#endif
			goto retc;
			}
 more96:
		rblo *= 10;
		rb = 10*rb + (rblo >> 32);
		rblo &= 0xffffffffull;
		if (rb >= 0x1000000000000000ull)
			goto Fast_failed1;
		reslo *= 10;
		res = 10*res + (reslo >> 32);
		reslo &= 0xffffffffull;
		ulp *= 5;
		if (ulp & 0x8000000000000000ull) {
			eulp += 4;
			ulp >>= 3;
			}
		else {
			eulp += 3;
			ulp >>= 2;
			}
		}
 Fast_failed1:
	Debug(++__dtoa_stats[6]);
	S = mhi = mlo = 0;
#ifdef USE_BF96
	b = __d2b(&u, &be, &bbits MTb);
#endif
	s = buf;
	i = (int)(word0(&u) >> Exp_shift1 & (Exp_mask>>Exp_shift1));
	i -= Bias;
	if (ulpadj)
		i -= ulpadj - 1;
	j = bbits - i - 1;
	if (j >= 0) {
		b2 = 0;
		s2 = j;
		}
	else {
		b2 = -j;
		s2 = 0;
		}
	if (k >= 0) {
		b5 = 0;
		s5 = k;
		s2 += k;
		}
	else {
		b2 -= k;
		b5 = -k;
		s5 = 0;
		}
#endif /*}*/

#ifdef Honor_FLT_ROUNDS
	if (mode > 1 && Rounding != 1)
		leftright = 0;
#endif

#ifndef USE_BF96 /*{*/
	if (ilim >= 0 && ilim <= Quick_max && try_quick) {

		/* Try to get by with floating-point arithmetic. */

		i = 0;
		dval(&d2) = dval(&u);
		j1 = -(k0 = k);
		ilim0 = ilim;
		ieps = 2; /* conservative */
		if (k > 0) {
			ds = tens[k&0xf];
			j = k >> 4;
			if (j & Bletch) {
				/* prevent overflows */
				j &= Bletch - 1;
				dval(&u) /= bigtens[n_bigtens-1];
				ieps++;
				}
			for(; j; j >>= 1, i++)
				if (j & 1) {
					ieps++;
					ds *= bigtens[i];
					}
			dval(&u) /= ds;
			}
		else if (j1 > 0) {
			dval(&u) *= tens[j1 & 0xf];
			for(j = j1 >> 4; j; j >>= 1, i++)
				if (j & 1) {
					ieps++;
					dval(&u) *= bigtens[i];
					}
			}
		if (k_check && dval(&u) < 1. && ilim > 0) {
			if (ilim1 <= 0)
				goto fast_failed;
			ilim = ilim1;
			k--;
			dval(&u) *= 10.;
			ieps++;
			}
		dval(&eps) = ieps*dval(&u) + 7.;
		word0(&eps) -= (P-1)*Exp_msk1;
		if (ilim == 0) {
			S = mhi = 0;
			dval(&u) -= 5.;
			if (dval(&u) > dval(&eps))
				goto one_digit;
			if (dval(&u) < -dval(&eps))
				goto no_digits;
			goto fast_failed;
			}
#ifndef No_leftright
		if (leftright) {
			/* Use Steele & White method of only
			 * generating digits needed.
			 */
			dval(&eps) = 0.5/tens[ilim-1] - dval(&eps);
#ifdef IEEE_Arith
			if (j1 >= 307) {
				eps1.d = 1.01e256; /* 1.01 allows roundoff in the next few lines */
				word0(&eps1) -= Exp_msk1 * (Bias+P-1);
				dval(&eps1) *= tens[j1 & 0xf];
				for(i = 0, j = (j1-256) >> 4; j; j >>= 1, i++)
					if (j & 1)
						dval(&eps1) *= bigtens[i];
				if (eps.d < eps1.d)
					eps.d = eps1.d;
				if (10. - u.d < 10.*eps.d && eps.d < 1.) {
					/* eps.d < 1. excludes trouble with the tiniest denormal */
					*s++ = '1';
					++k;
					goto ret1;
					}
				}
#endif
			for(i = 0;;) {
				L = dval(&u);
				dval(&u) -= L;
				*s++ = '0' + (int)L;
				if (1. - dval(&u) < dval(&eps))
					goto bump_up;
				if (dval(&u) < dval(&eps))
					goto retc;
				if (++i >= ilim)
					break;
				dval(&eps) *= 10.;
				dval(&u) *= 10.;
				}
			}
		else {
#endif
			/* Generate ilim digits, then fix them up. */
			dval(&eps) *= tens[ilim-1];
			for(i = 1;; i++, dval(&u) *= 10.) {
				L = (Long)(dval(&u));
				if (!(dval(&u) -= L))
					ilim = i;
				*s++ = '0' + (int)L;
				if (i == ilim) {
					if (dval(&u) > 0.5 + dval(&eps))
						goto bump_up;
					else if (dval(&u) < 0.5 - dval(&eps))
						goto retc;
					break;
					}
				}
#ifndef No_leftright
			}
#endif
 fast_failed:
		s = buf;
		dval(&u) = dval(&d2);
		k = k0;
		ilim = ilim0;
		}

	/* Do we have a "small" integer? */

	if (be >= 0 && k <= Int_max) {
		/* Yes. */
		ds = tens[k];
		if (ndigits < 0 && ilim <= 0) {
			S = mhi = 0;
			if (ilim < 0 || dval(&u) <= 5*ds)
				goto no_digits;
			goto one_digit;
			}
		for(i = 1;; i++, dval(&u) *= 10.) {
			L = (Long)(dval(&u) / ds);
			dval(&u) -= L*ds;
#ifdef Check_FLT_ROUNDS
			/* If FLT_ROUNDS == 2, L will usually be high by 1 */
			if (dval(&u) < 0) {
				L--;
				dval(&u) += ds;
				}
#endif
			*s++ = '0' + (int)L;
			if (!dval(&u)) {
#ifdef SET_INEXACT
				inexact = 0;
#endif
				break;
				}
			if (i == ilim) {
#ifdef Honor_FLT_ROUNDS
				if (mode > 1)
				switch(Rounding) {
				  case 0: goto retc;
				  case 2: goto bump_up;
				  }
#endif
				dval(&u) += dval(&u);
#ifdef ROUND_BIASED
				if (dval(&u) >= ds)
#else
				if (dval(&u) > ds || (dval(&u) == ds && L & 1))
#endif
					{
 bump_up:
					while(*--s == '9')
						if (s == buf) {
							k++;
							*s = '0';
							break;
							}
					++*s++;
					}
				break;
				}
			}
		goto retc;
		}

#endif /*}*/
	m2 = b2;
	m5 = b5;
	mhi = mlo = 0;
	if (leftright) {
		i =
#ifndef Sudden_Underflow
			denorm ? be + (Bias + (P-1) - 1 + 1) :
#endif
#ifdef IBM
			1 + 4*P - 3 - bbits + ((bbits + be - 1) & 3);
#else
			1 + P - bbits;
#endif
		b2 += i;
		s2 += i;
		mhi = __i2b(1 MTb);
		}
	if (m2 > 0 && s2 > 0) {
		i = m2 < s2 ? m2 : s2;
		b2 -= i;
		m2 -= i;
		s2 -= i;
		}
	if (b5 > 0) {
		if (leftright) {
			if (m5 > 0) {
				mhi = __pow5mult(mhi, m5 MTb);
				b1 = __mult(mhi, b MTb);
				__Bfree(b MTb);
				b = b1;
				}
			if ((j = b5 - m5))
				b = __pow5mult(b, j MTb);
			}
		else
			b = __pow5mult(b, b5 MTb);
		}
	S = __i2b(1 MTb);
	if (s5 > 0)
		S = __pow5mult(S, s5 MTb);

	if (spec_case) {
		b2 += Log2P;
		s2 += Log2P;
		}

	/* Arrange for convenient computation of quotients:
	 * shift left if necessary so divisor has 4 leading 0 bits.
	 *
	 * Perhaps we should just compute leading 28 bits of S once
	 * and for all and pass them and a shift to __quorem, so it
	 * can do shifts and ors to compute the numerator for q.
	 */
	i = __dshift(S, s2);
	b2 += i;
	m2 += i;
	s2 += i;
	if (b2 > 0)
		b = __lshift(b, b2 MTb);
	if (s2 > 0)
		S = __lshift(S, s2 MTb);
#ifndef USE_BF96
	if (k_check) {
		if (__cmp(b,S) < 0) {
			k--;
			b = __multadd(b, 10, 0 MTb);	/* we botched the k estimate */
			if (leftright)
				mhi = __multadd(mhi, 10, 0 MTb);
			ilim = ilim1;
			}
		}
#endif
	if (ilim <= 0 && (mode == 3 || mode == 5)) {
		if (ilim < 0 || __cmp(b,S = __multadd(S,5,0 MTb)) <= 0) {
			/* no digits, fcvt style */
 no_digits:
			k = -1 - ndigits;
			goto ret;
			}
 one_digit:
		*s++ = '1';
		++k;
		goto ret;
		}
	if (leftright) {
		if (m2 > 0)
			mhi = __lshift(mhi, m2 MTb);

		/* Compute mlo -- check for special case
		 * that d is a normalized power of 2.
		 */

		mlo = mhi;
		if (spec_case) {
			mhi = __Balloc(mhi->k MTb);
			Bcopy(mhi, mlo);
			mhi = __lshift(mhi, Log2P MTb);
			}

		for(i = 1;;i++) {
			dig = __quorem(b,S) + '0';
			/* Do we yet have the shortest decimal string
			 * that will round to d?
			 */
			j = __cmp(b, mlo);
			delta = __diff(S, mhi MTb);
			j1 = delta->sign ? 1 : __cmp(b, delta);
			__Bfree(delta MTb);
#ifndef ROUND_BIASED
			if (j1 == 0 && mode != 1 && !(word1(&u) & 1)
#ifdef Honor_FLT_ROUNDS
				&& (mode <= 1 || Rounding >= 1)
#endif
								   ) {
				if (dig == '9')
					goto round_9_up;
				if (j > 0)
					dig++;
#ifdef SET_INEXACT
				else if (!b->x[0] && b->wds <= 1)
					inexact = 0;
#endif
				*s++ = dig;
				goto ret;
				}
#endif
			if (j < 0 || (j == 0 && mode != 1
#ifndef ROUND_BIASED
							&& !(word1(&u) & 1)
#endif
					)) {
				if (!b->x[0] && b->wds <= 1) {
#ifdef SET_INEXACT
					inexact = 0;
#endif
					goto accept_dig;
					}
#ifdef Honor_FLT_ROUNDS
				if (mode > 1)
				 switch(Rounding) {
				  case 0: goto accept_dig;
				  case 2: goto keep_dig;
				  }
#endif /*Honor_FLT_ROUNDS*/
				if (j1 > 0) {
					b = __lshift(b, 1 MTb);
					j1 = __cmp(b, S);
#ifdef ROUND_BIASED
					if (j1 >= 0 /*)*/
#else
					if ((j1 > 0 || (j1 == 0 && dig & 1))
#endif
					&& dig++ == '9')
						goto round_9_up;
					}
 accept_dig:
				*s++ = dig;
				goto ret;
				}
			if (j1 > 0) {
#ifdef Honor_FLT_ROUNDS
				if (!Rounding && mode > 1)
					goto accept_dig;
#endif
				if (dig == '9') { /* possible if i == 1 */
 round_9_up:
					*s++ = '9';
					goto roundoff;
					}
				*s++ = dig + 1;
				goto ret;
				}
#ifdef Honor_FLT_ROUNDS
 keep_dig:
#endif
			*s++ = dig;
			if (i == ilim)
				break;
			b = __multadd(b, 10, 0 MTb);
			if (mlo == mhi)
				mlo = mhi = __multadd(mhi, 10, 0 MTb);
			else {
				mlo = __multadd(mlo, 10, 0 MTb);
				mhi = __multadd(mhi, 10, 0 MTb);
				}
			}
		}
	else
		for(i = 1;; i++) {
			dig = __quorem(b,S) + '0';
			*s++ = dig;
			if (!b->x[0] && b->wds <= 1) {
#ifdef SET_INEXACT
				inexact = 0;
#endif
				goto ret;
				}
			if (i >= ilim)
				break;
			b = __multadd(b, 10, 0 MTb);
			}

	/* Round off last digit */

#ifdef Honor_FLT_ROUNDS
	if (mode > 1)
		switch(Rounding) {
		  case 0: goto ret;
		  case 2: goto roundoff;
		  }
#endif
	b = __lshift(b, 1 MTb);
	j = __cmp(b, S);
#ifdef ROUND_BIASED
	if (j >= 0)
#else
	if (j > 0 || (j == 0 && dig & 1))
#endif
		{
 roundoff:
		while(*--s == '9')
			if (s == buf) {
				k++;
				*s++ = '1';
				goto ret;
				}
		++*s++;
		}
 ret:
	__Bfree(S MTb);
	if (mhi) {
		if (mlo && mlo != mhi)
			__Bfree(mlo MTb);
		__Bfree(mhi MTb);
		}
 retc:
	while(s > buf && s[-1] == '0')
		--s;
 ret1:
	if (b)
		__Bfree(b MTb);
	*s = 0;
	*decpt = k + 1;
	if (rve)
		*rve = s;
#ifdef SET_INEXACT
	if (inexact) {
		if (!oldinexact) {
			word0(&u) = Exp_1 + (70 << Exp_shift);
			word1(&u) = 0;
			dval(&u) += 1.;
			}
		}
	else if (!oldinexact)
		clear_inexact();
#endif
	return buf;
	}

 char *
dtoa(double dd, int mode, int ndigits, int *decpt, int *sign, char **rve)
{
	/*	Sufficient space is allocated to the return value
		to hold the suppressed trailing zeros.
		See __dtoa_r() above for details on the other arguments.
	*/
#ifndef MULTIPLE_THREADS
	if (dtoa_result)
		freedtoa(dtoa_result);
#endif
	return __dtoa_r(dd, mode, ndigits, decpt, sign, rve, 0, 0);
	}
