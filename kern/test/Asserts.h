//
//  Asserts.h
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Asserts_h
#define Asserts_h 1

#include <_cmndef.h>
#include <stdio.h>

__CPP_BEGIN

extern void Assert(const char* _Nonnull pFuncname, int lineNum, const char* _Nonnull fmt, ...);

#define assertEOF(cond)     if ((cond) != EOF) { Assert(__func__, __LINE__, "%s", #cond); }
#define assertNotEOF(cond)  if ((cond) == EOF) { Assert(__func__, __LINE__, "%s", #cond); }

#define assertNULL(cond) if ((cond) != NULL) { Assert(__func__, __LINE__, "%s", #cond); }
#define assertNotNULL(cond) if ((cond) == NULL) { Assert(__func__, __LINE__, "%s", #cond); }

#define assertZero(cond) if ((cond) != 0) { Assert(__func__, __LINE__, "%s", #cond); }
#define assertNotZero(cond) if ((cond) == 0) { Assert(__func__, __LINE__, "%s", #cond); }

#define assertOK(cond) if ((cond) != 0) { Assert(__func__, __LINE__, "%s", #cond); }
#define assertEquals(expected, actual) if ((expected) != (actual)) { Assert(__func__, __LINE__, "" #expected " vs %d", (int)(actual)); }
#define assertGreaterEqual(expected, actual) if ((expected) > (actual)) { Assert(__func__, __LINE__, "" #expected " vs %d", (int)(actual)); }

#define assertTrue(actual) if (!(actual)) { Assert(__func__, __LINE__, "%s", #actual); }
#define assertFalse(actual) if (actual) { Assert(__func__, __LINE__, "%s", #actual); }

__CPP_END

#endif /* Asserts_h */
