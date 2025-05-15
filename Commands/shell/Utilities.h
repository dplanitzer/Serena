//
//  Utilities.h
//  sh
//
//  Created by Dietmar Planitzer on 5/20/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Utilities_h
#define Utilities_h

#include "Errors.h"
#include <stdlib.h>
#include <System/_math.h>

#define exit_code(__err) \
((__err) == EOK) ? EXIT_SUCCESS : EXIT_FAILURE

extern void print_error(const char* _Nonnull cmd, const char* _Nullable path, errno_t err);
extern const char* char_to_ascii(char ch, char buf[3]);
extern size_t hash_cstring(const char* _Nonnull str);
extern size_t hash_string(const char* _Nonnull str, size_t len);
extern errno_t read_contents_of_file(const char* _Nonnull path, char* _Nullable * _Nonnull pOutText, size_t* _Nullable pOutLength);

#endif  /* Utilities_h */
