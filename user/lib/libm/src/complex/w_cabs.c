/*
 * cabs() wrapper for hypot().
 *
 * Written by J.T. Conklin, <jtc@wimsey.com>
 * Placed into the Public Domain, 1994.
 */

#include "cdefs-compat.h"
//__FBSDID("$FreeBSD: src/lib/msun/src/w_cabs.c,v 1.7 2008/03/30 20:03:06 das Exp $");

#include <float.h>
#include <complex.h>
#include <math.h>

#include "math_private.h"

#ifndef __VBCC__
OLM_DLLEXPORT double
cabs(double complex z)
{
	return hypot(creal(z), cimag(z));
}

#if LDBL_MANT_DIG == 53
openlibm_weak_reference(cabs, cabsl);
#endif
#endif
