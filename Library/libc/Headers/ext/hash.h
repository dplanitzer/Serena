//
//  ext/hash.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 3/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_HASH_H
#define _EXT_HASH_H

#include <_cmndef.h>
#include <stddef.h>

__CPP_BEGIN

//
// General hash functions
//

#define hash_scalar(__val)  hash_djb2_scalar(__val)
#define hash_string(__str)  hash_djb2_string(__str)
#define hash_bytes(__bytes, __len)  hash_djb2_bytes(__bytes, __len)


//
// DJB2 hash functions
//
// ~: hash * 33 + val with hash set to some initial seed value
//
// All hash functions here avoid doing multiplications and divisions for
// performance reasons.
//
// Hash values are represented by 'size_t'.
//

#define hash_djb2_scalar(__val) \
(4521 + (size_t)(__val))

extern size_t hash_djb2_string(const char* _Nonnull str);
extern size_t hash_djb2_bytes(const void* _Nonnull bytes, size_t len);

__CPP_END

#endif /* _EXT_HASH_H */
