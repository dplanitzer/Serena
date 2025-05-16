//
//  sys/endian.h
//  libc
//
//  Created by Dietmar Planitzer on 3/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_ENDIAN_H
#define _SYS_ENDIAN_H 1

#include <System/_cmndef.h>
#include <stdint.h>

__CPP_BEGIN

#if defined(__M68K__) || defined(__BIG_ENDIAN__)
#define __BYTE_ORDER_BIG__ 1
#elif defined(_M_IX86) || defined(_M_X64) || defined(__x86_64__) || defined(__i386__) || defined(__LITTLE_ENDIAN__)
#define __BYTE_ORDER_LITTLE__ 1
#else
#error "Unknown platform"
#endif


inline uint16_t UInt16_Swap(uint16_t w) {
    return (w << 8) | (w >> 8);
}

inline uint32_t UInt32_Swap(uint32_t w) {
    w = ((w << 8) & 0xff00ff00) | ((w >> 8) & 0xff00ff); 
    return (w << 16) | (w >> 16);
}

inline uint64_t UInt64_Swap(uint64_t w) {
    w = ((w << 8) & 0xff00ff00ff00ff00ull) | ((w >> 8) & 0x00ff00ff00ff00ffull);
    w = ((w << 16) & 0xffff0000ffff0000ull) | ((w >> 16) & 0x0000ffff0000ffffull);
    return (w << 32) | (w >> 32);
}


#ifdef __BYTE_ORDER_BIG__

#define Int16_HostToBig(__w) (int16_t)(__w)
#define Int32_HostToBig(__w) (int32_t)(__w)
#define Int64_HostToBig(__w) (int64_t)(__w)

#define UInt16_HostToBig(__w) (uint16_t)(__w)
#define UInt32_HostToBig(__w) (uint32_t)(__w)
#define UInt64_HostToBig(__w) (uint64_t)(__w)

#define Int16_BigToHost(__w) (int16_t)(__w)
#define Int32_BigToHost(__w) (int32_t)(__w)
#define Int64_BigToHost(__w) (int64_t)(__w)

#define UInt16_BigToHost(__w) (uint16_t)(__w)
#define UInt32_BigToHost(__w) (uint32_t)(__w)
#define UInt64_BigToHost(__w) (uint64_t)(__w)


#define Int16_HostToLittle(__w) (int16_t)UInt16_Swap((uint16_t)__w)
#define Int32_HostToLittle(__w) (int32_t)UInt32_Swap((uint32_t)__w)
#define Int64_HostToLittle(__w) (int64_t)UInt64_Swap((uint64_t)__w)

#define UInt16_HostToLittle(__w) UInt16_Swap(__w)
#define UInt32_HostToLittle(__w) UInt32_Swap(__w)
#define UInt64_HostToLittle(__w) UInt64_Swap(__w)

#define Int16_LittleToHost(__w) (int16_t)UInt16_Swap((uint16_t)__w)
#define Int32_LittleToHost(__w) (int32_t)UInt32_Swap((uint32_t)__w)
#define Int64_LittleToHost(__w) (int64_t)UInt64_Swap((uint64_t)__w)

#define UInt16_LittleToHost(__w) UInt16_Swap(__w)
#define UInt32_LittleToHost(__w) UInt32_Swap(__w)
#define UInt64_LittleToHost(__w) UInt64_Swap(__w)

#else

#define Int16_HostToBig(__w) (int16_t)UInt16_Swap((uint16_t)__w)
#define Int32_HostToBig(__w) (int32_t)UInt32_Swap((uint32_t)__w)
#define Int64_HostToBig(__w) (int64_t)UInt64_Swap((uint64_t)__w)

#define UInt16_HostToBig(__w) UInt16_Swap(__w)
#define UInt32_HostToBig(__w) UInt32_Swap(__w)
#define UInt64_HostToBig(__w) UInt64_Swap(__w)

#define Int16_BigToHost(__w) (int16_t)UInt16_Swap((uint16_t)__w)
#define Int32_BigToHost(__w) (int32_t)UInt32_Swap((uint32_t)__w)
#define Int64_BigToHost(__w) (int64_t)UInt64_Swap((uint64_t)__w)

#define UInt16_BigToHost(__w) UInt16_Swap(__w)
#define UInt32_BigToHost(__w) UInt32_Swap(__w)
#define UInt64_BigToHost(__w) UInt64_Swap(__w)


#define Int16_HostToLittle(__w) (int16_t)(__w)
#define Int32_HostToLittle(__w) (int32_t)(__w)
#define Int64_HostToLittle(__w) (int64_t)(__w)

#define UInt16_HostToLittle(__w) (uint16_t)(__w)
#define UInt32_HostToLittle(__w) (uint32_t)(__w)
#define UInt64_HostToLittle(__w) (uint64_t)(__w)

#define Int16_LittleToHost(__w) (int16_t)(__w)
#define Int32_LittleToHost(__w) (int32_t)(__w)
#define Int64_LittleToHost(__w) (int64_t)(__w)

#define UInt16_LittleToHost(__w) (uint16_t)(__w)
#define UInt32_LittleToHost(__w) (uint32_t)(__w)
#define UInt64_LittleToHost(__w) (uint64_t)(__w)

#endif

__CPP_END

#endif /* _SYS_ENDIAN_H */
