//
//  ext/endian.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 3/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_ENDIAN_H
#define _EXT_ENDIAN_H 1

#include <ext/byteswap.h>

__CPP_BEGIN

#if defined(__M68K__) || defined(__BIG_ENDIAN__)
#define __BYTE_ORDER_BIG__ 1
#elif defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386__) || defined(__LITTLE_ENDIAN__)
#define __BYTE_ORDER_LITTLE__ 1
#else
#error "Unknown platform"
#endif


#ifdef __BYTE_ORDER_BIG__

#define htobe16(__w) (__w)
#define htobe32(__w) (__w)
#define htobe64(__w) (__w)

#define be16toh(__w) (__w)
#define be32toh(__w) (__w)
#define be64toh(__w) (__w)


#define htole16(__w) bswap_16(__w)
#define htole32(__w) bswap_32(__w)
#define htole64(__w) bswap_64(__w)

#define le16toh(__w) bswap_16(__w)
#define le32toh(__w) bswap_32(__w)
#define le64toh(__w) bswap_64(__w)

#else

#define htobe16(__w) bswap_16(__w)
#define htobe32(__w) bswap_32(__w)
#define htobe64(__w) bswap_64(__w)

#define be16toh(__w) bswap_16(__w)
#define be32toh(__w) bswap_32(__w)
#define be64toh(__w) bswap_64(__w)


#define htole16(__w) (__w)
#define htole32(__w) (__w)
#define htole64(__w) (__w)

#define le16toh(__w) (__w)
#define le32toh(__w) (__w)
#define le64toh(__w) (__w)

#endif

__CPP_END

#endif /* _EXT_ENDIAN_H */
