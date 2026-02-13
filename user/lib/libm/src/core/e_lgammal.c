#ifdef OLM_LONG_DOUBLE
#include "cdefs-compat.h"

#include <math.h>

#include "math_private.h"

OLM_DLLEXPORT long double
lgammal(long double x)
{
#ifdef OPENLIBM_ONLY_THREAD_SAFE
	int signgam;
#endif

	return (lgammal_r(x, &signgam));
}
#endif
