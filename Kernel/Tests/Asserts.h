//
//  Asserts.h
//  Kernel Tests
//
//  Created by Dietmar Planitzer on 1/30/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Asserts_h
#define Asserts_h 1

#include <System/_cmndef.h>
#include <System/Error.h>
#include <stdio.h>

__CPP_BEGIN

extern void Assert(const char* _Nonnull pFuncname, int lineNum, const char* _Nonnull expr);

#define assertEOF(cond)     if ((cond) != EOF) { Assert(__func__, __LINE__, #cond); }
#define assertNotEOF(cond)  if ((cond) == EOF) { Assert(__func__, __LINE__, #cond); }

#define assertNULL(cond) if ((cond) != NULL) { Assert(__func__, __LINE__, #cond); }
#define assertNotNULL(cond) if ((cond) == NULL) { Assert(__func__, __LINE__, #cond); }

#define assertZero(cond) if ((cond) != 0) { Assert(__func__, __LINE__, #cond); }
#define assertNotZero(cond) if ((cond) == 0) { Assert(__func__, __LINE__, #cond); }

#define assertOK(cond) if ((cond) != 0) { Assert(__func__, __LINE__, #cond); }
#define assertEquals(expected, actual) if ((expected) != (actual)) { Assert(__func__, __LINE__, #expected); }

#define assertTrue(actual) if (!(actual)) { Assert(__func__, __LINE__, #actual); }
#define assertFalse(actual) if (actual) { Assert(__func__, __LINE__, #actual); }

__CPP_END

#endif /* Asserts_h */
