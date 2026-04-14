//
//  ext/byteswap.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 4/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _EXT_BYTESWAP_H
#define _EXT_BYTESWAP_H 1

#include <_cmndef.h>
#include <stdint.h>

__CPP_BEGIN


static inline uint16_t bswap_16(uint16_t w) {
    return (w << 8) | (w >> 8);
}

static inline uint32_t bswap_32(uint32_t w) {
    w = ((w << 8) & 0xff00ff00) | ((w >> 8) & 0xff00ff); 
    return (w << 16) | (w >> 16);
}

static inline uint64_t bswap_64(uint64_t w) {
    w = ((w << 8) & 0xff00ff00ff00ff00ull) | ((w >> 8) & 0x00ff00ff00ff00ffull);
    w = ((w << 16) & 0xffff0000ffff0000ull) | ((w >> 16) & 0x0000ffff0000ffffull);
    return (w << 32) | (w >> 32);
}

__CPP_END

#endif /* _EXT_BYTESWAP_H */
