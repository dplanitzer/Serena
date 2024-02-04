//
//  Int64.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/2/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//
//
// nlz() and divmnu() are taken from the book Hacker's Delight,
// 2nd Edition by Henry S Warren, Jr.
//
// The reference code from the book can be found at:
// https://github.com/hcs0/Hackers-Delight/blob/master/divmnu.c.txt
// http://www.hackersdelight.org/hdcodetxt/divmnu.c.txt
//

extern int __divmodi64(long long dividend, long long divisor, long long* quotient, long long* remainder);


#define NULL    ((void*)0)

typedef union i64 {
    long long i;
    unsigned short s[4];
} i64_t;

static int __nlz(unsigned int x)
{
    int n;

    if (x == 0) return(32);
    n = 0;
    if (x <= 0x0000FFFF) {n = n +16; x = x <<16;}
    if (x <= 0x00FFFFFF) {n = n + 8; x = x << 8;}
    if (x <= 0x0FFFFFFF) {n = n + 4; x = x << 4;}
    if (x <= 0x3FFFFFFF) {n = n + 2; x = x << 2;}
    if (x <= 0x7FFFFFFF) {n = n + 1;}
    return n;
}

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

static int __divmnu(unsigned short  * q, unsigned short  * r,
                 const unsigned short  * u, const unsigned short  * v,
           int m, int n)
{

    const unsigned int b = 65536;   // Number base (16 bits).
    unsigned short un[10];          // Normalized form of u. 2n
    unsigned short vn[8];           // Normalized form of v. 2(m+1)
    unsigned int qhat;              // Estimated quotient digit.
    unsigned int rhat;              // A remainder.
    unsigned int p;                 // Product of two digits.
    int s, i, j, t, k;

    if (m < n || n <= 0 || v[n-1] == 0)
        return 1;                   // Return if invalid param.

    if (n == 1) {                        // Take care of
        k = 0;                            // the case of a
        for (j = m - 1; j >= 0; j--) {    // single-digit
            q[j] = (k*b + u[j])/v[0];      // divisor here.
            k = (k*b + u[j]) - q[j]*v[0];
        }
        if (r != NULL) r[0] = k;
        return 0;
    }

    // Normalize by shifting v left just enough so that
    // its high-order bit is on, and shift u left the
    // same amount.  We may have to append a high-order
    // digit on the dividend; we do that unconditionally.

    s = __nlz(v[n-1]) - 16;        // 0 <= s <= 15.
    for (i = n - 1; i > 0; i--)
        vn[i] = (v[i] << s) | (v[i-1] >> (16-s));
    vn[0] = v[0] << s;

    un[m] = u[m-1] >> (16-s);
    for (i = m - 1; i > 0; i--)
        un[i] = (u[i] << s) | (u[i-1] >> (16-s));
    un[0] = u[0] << s;

    for (j = m - n; j >= 0; j--) {       // Main loop.
        // Compute estimate qhat of q[j].
        qhat = (un[j+n]*b + un[j+n-1])/vn[n-1];
        rhat = (un[j+n]*b + un[j+n-1]) - qhat*vn[n-1];
    again:
        if (qhat >= b || qhat*vn[n-2] > b*rhat + un[j+n-2])
        { qhat = qhat - 1;
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
    if (r != NULL) {
        for (i = 0; i < n; i++)
            r[i] = (un[i] >> s) | (un[i+1] << (16-s));
    }
    return 0;
}


int __divmodi64(long long dividend, long long divisor, long long* quotient, long long* remainder)
{
    i64_t x, y, qx, rx;
    unsigned short q[4], r[4];
    unsigned short u[4], v[4];
    int m = 4, n = 4;

    if (dividend == 0) {
        if (quotient) *quotient = 0;
        if (remainder) *remainder = 0;
        return 0;
    }

    x.i = dividend;
    y.i = divisor;

#if __BIG_ENDIAN__
    for (int i = 0; i < 4; i++) {
        if (x.s[i]) break;
        m--;
    }
    for (int i = 0; i < 4; i++) {
        if (y.s[i]) break;
        n--;
    }

    for (int i = 0; i < m; i++) {
        u[i] = x.s[3 - i];
    }

    for (int i = 0; i < n; i++) {
        v[i] = y.s[3 - i];
    }
#else
    for (int i = 3; i >= 0; i--) {
        if (x.s[i]) break;
        m--;
    }
    for (int i = 3; i >= 0; i--) {
        if (y.s[i]) break;
        n--;
    }

    for (int i = 0; i < m; i++) {
        u[i] = x.s[i];
    }

    for (int i = 0; i < n; i++) {
        v[i] = y.s[i];
    }
#endif

    const int err = __divmnu(q, r, u, v, m, n);
    if (err != 0) {
        if (quotient) *quotient = 0;
        if (remainder) *remainder = 0;
        return err;
    }
    
    if (quotient) {
        qx.i = 0LL;
        for (int i = 0; i < m - n + 1; i++) {
#if __BIG_ENDIAN__
            qx.s[3 - i] = q[i];
#else
            qx.s[i] = q[i];
#endif
        }

        *quotient = qx.i;
    }

    if (remainder) {
        rx.i = 0LL;
        for (int i = 0; i < n; i++) {
#if __BIG_ENDIAN__
            rx.s[3 - i] = r[i];
#else
            rx.s[i] = r[i];
#endif
        }

        *remainder = rx.i;
    }

    return 0;
}


// The code that the vbcc C compiler generates expects the following functions
// to exist
long long _divsint64_020(long long dividend, long long divisor)
{
    long long quo;
    
    __divmodi64(dividend, divisor, &quo, NULL);
    
    return quo;
}

long long _divsint64_060(long long dividend, long long divisor)
{
    long long quo;
    
    __divmodi64(dividend, divisor, &quo, NULL);
    
    return quo;
}

long long _modsint64_020(long long dividend, long long divisor)
{
    long long quo, rem;
    
    __divmodi64(dividend, divisor, &quo, &rem);
    
    return rem;
}

long long _modsint64_060(long long dividend, long long divisor)
{
    long long quo, rem;
    
    __divmodi64(dividend, divisor, &quo, &rem);
    
    return rem;
}

unsigned long long _divuint64_20(unsigned long long dividend, unsigned long long divisor)
{
    long long quo;
    
    __divmodi64(dividend, divisor, &quo, NULL);
    
    return (unsigned long long) quo;
}

unsigned long long _moduint64_20(unsigned long long dividend, unsigned long long divisor)
{
    long long quo, rem;
    
    __divmodi64(dividend, divisor, &quo, &rem);
    
    return (unsigned long long) rem;
}
