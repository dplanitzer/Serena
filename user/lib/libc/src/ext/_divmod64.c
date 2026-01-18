//
//  arch/m68k/_divmod64.c
//  libsc
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//
//
// divmnu() are taken from the book Hacker's Delight, 2nd Edition by Henry S Warren, Jr.
//
// The reference code from the book can be found at:
// https://github.com/hcs0/Hackers-Delight/blob/master/divmnu.c.txt
// http://www.hackersdelight.org/hdcodetxt/divmnu.c.txt
//

#include <__crt.h>
#include <stdbool.h>
#include <stddef.h>
#include <ext/bit.h>
#include <ext/math.h>


/* q[0], r[0], u[0], and v[0] contain the LEAST significant halfwords.
 (The sequence is in little-endian order).

 This first version is a fairly precise implementation of Knuth's
 Algorithm D, for a binary computer with base b = 2**16.  The caller
 supplies
 1. Space q for the quotient, m - n + 1 halfwords (at least one).
 2. Space r for the remainder (optional), n halfwords.
 3. The dividend u, m halfwords, m >= 1.
 4. The divisor v, n halfwords, n >= 2.
 The most significant digit of the divisor, v[n-1], must be nonzero.  The
 dividend u may have leading zeros; this just makes the algorithm take
 longer and makes the quotient contain more leading zeros.  A value of
 NULL may be given for the address of the remainder to signify that the
 caller does not want the remainder.
 The program does not alter the input parameters u and v.
 The quotient and remainder returned may have leading zeros.  The
 function itself returns a value of 0 for success and 1 for invalid
 parameters (e.g., division by 0).
 For now, we must have m >= n.  Knuth's Algorithm D also requires
 that the dividend be at least as long as the divisor.  (In his terms,
 m >= 0 (unstated).  Therefore m+n >= n.) */

static int __divmnu(
    unsigned short* _Nonnull _Restrict q, unsigned short* _Nullable _Restrict r,
    const unsigned short* _Nonnull _Restrict u, const unsigned short* _Nonnull _Restrict v,
    int m, int n
) {
    const unsigned int b = 65536;   // Number base (16 bits).
    unsigned short un[10];          // Normalized form of u. 2n
    unsigned short vn[8];           // Normalized form of v. 2(m+1)
    unsigned int qhat;              // Estimated quotient digit.
    unsigned int rhat;              // A remainder.
    unsigned int p;                 // Product of two digits.
    int s, i, j, t, k;

    if (m < n || n <= 0 || v[n-1] == 0) {
        return 1;                   // Return if invalid param.
    }

    if (n == 1) {                        // Take care of
        k = 0;                            // the case of a
        for (j = m - 1; j >= 0; j--) {    // single-digit
            q[j] = (k*b + u[j])/v[0];      // divisor here.
            k = (k*b + u[j]) - q[j]*v[0];
        }
        if (r) r[0] = k;
        return 0;
    }

    // Normalize by shifting v left just enough so that
    // its high-order bit is on, and shift u left the
    // same amount.  We may have to append a high-order
    // digit on the dividend; we do that unconditionally.

    s = leading_zeros_us(v[n-1]);        // 0 <= s <= 15.
    for (i = n - 1; i > 0; i--) {
        vn[i] = (v[i] << s) | (v[i-1] >> (16-s));
    }
    vn[0] = v[0] << s;

    un[m] = u[m-1] >> (16-s);
    for (i = m - 1; i > 0; i--) {
        un[i] = (u[i] << s) | (u[i-1] >> (16-s));
    }
    un[0] = u[0] << s;

    for (j = m - n; j >= 0; j--) {       // Main loop.
        // Compute estimate qhat of q[j].
        qhat = (un[j+n]*b + un[j+n-1])/vn[n-1];
        rhat = (un[j+n]*b + un[j+n-1]) - qhat*vn[n-1];
    again:
        if (qhat >= b || qhat*vn[n-2] > b*rhat + un[j+n-2]) {
            qhat = qhat - 1;
            rhat = rhat + vn[n-1];
            if (rhat < b) goto again;
        }

        // Multiply and subtract.
        k = 0;
        for (i = 0; i < n; i++) {
            p = qhat*vn[i];
            t = un[i+j] - k - (p & 0xFFFF);
            un[i+j] = t;
            k = (p >> 16) - (t >> 16);
        }
        t = un[j+n] - k;
        un[j+n] = t;

        q[j] = qhat;              // Store quotient digit.
        if (t < 0) {              // If we subtracted too
            q[j] = q[j] - 1;       // much, add back.
            k = 0;
            for (i = 0; i < n; i++) {
                t = un[i+j] + vn[i] + k;
                un[i+j] = t;
                k = t >> 16;
            }
            un[j+n] = un[j+n] + k;
        }
    } // End j.

    // If the caller wants the remainder, unnormalize
    // it and pass it back.
    if (r) {
        for (i = 0; i < n; i++) {
            r[i] = (un[i] >> s) | (un[i+1] << (16-s));
        }
    }
    return 0;
}

#define DIVIDEND    0
#define DIVISOR     1
void _divmodu64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
{
    unsigned short q[4], r[4];
    unsigned short u[4], v[4];
    const iu64_t* dividend = &dividend_divisor[DIVIDEND];
    const iu64_t* divisor = &dividend_divisor[DIVISOR];
    int m = 4, n = 4;

    if (dividend->u64 == 0ull) {
        quotient->u64 = 0ull;
        if (remainder) remainder->u64 = 0ull;
        return;
    }

#if __M68K__
    for (int i = 0; i < 4; i++) {
        if (dividend->u16[i]) break;
        m--;
    }
    for (int i = 0; i < 4; i++) {
        if (divisor->u16[i]) break;
        n--;
    }

    for (int i = 0; i < m; i++) {
        u[i] = dividend->u16[3 - i];
    }

    for (int i = 0; i < n; i++) {
        v[i] = divisor->u16[3 - i];
    }
#else
    for (int i = 3; i >= 0; i--) {
        if (dividend->u16[i]) break;
        m--;
    }
    for (int i = 3; i >= 0; i--) {
        if (divisor->u16[i]) break;
        n--;
    }

    for (int i = 0; i < m; i++) {
        u[i] = dividend->u16[i];
    }

    for (int i = 0; i < n; i++) {
        v[i] = divisor->u16[i];
    }
#endif

    if (__divmnu(q, r, u, v, m, n) != 0) {
        quotient->u64 = 0ull;
        if (remainder) remainder->u64 = 0ull;
        return;
    }
    
    quotient->u64 = 0ull;
    for (int i = 0; i < m - n + 1; i++) {
#if __M68K__
        quotient->u16[3 - i] = q[i];
#else
        quotient->u16[i] = q[i];
#endif
    }

    if (remainder) {
        remainder->u64 = 0ull;
        for (int i = 0; i < n; i++) {
#if __M68K__
            remainder->u16[3 - i] = r[i];
#else
            remainder->u16[i] = r[i];
#endif
        }
    }
}

void _divmods64(const iu64_t _Nonnull dividend_divisor[2], iu64_t* _Nonnull _Restrict quotient, iu64_t* _Nullable _Restrict remainder)
{
    iu64_t xy_u[2];
    const iu64_t* dividend = &dividend_divisor[DIVIDEND];
    const iu64_t* divisor = &dividend_divisor[DIVISOR];
    const bool q_neg = (dividend->s64 < 0ll) ^ (divisor->s64 < 0ll);
    const bool r_neg = (dividend->s64 < 0ll);

    xy_u[DIVIDEND].u64 = __abs(dividend->s64);
    xy_u[DIVISOR].u64 = __abs(divisor->s64);

    _divmodu64(xy_u, quotient, remainder);

    if (q_neg) {
        quotient->s64 = -quotient->u64;
    }

    if (remainder) {
        if (r_neg) {
            remainder->s64 = -remainder->u64;
        }
    }
}
