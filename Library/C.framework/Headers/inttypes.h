//
//  inttypes.h
//  Apollo
//
//  Created by Dietmar Planitzer on 8/29/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef _INTTYPES_H
#define _INTTYPES_H 1

#define PRId8 hhd
#define PRId16 hd
#define PRId32 ld
#define PRId64 lld

#define PRIi8 PRId8
#define PRIi16 PRId16
#define PRIi32 PRId32
#define PRIi64 PRId64

#define PRIu8 hhu
#define PRIu16 hu
#define PRIu32 lu
#define PRIu64 llu

#define PRIo8 hho
#define PRIo16 ho
#define PRIo32 lo
#define PRIo64 llo

#define PRIx8 hhx
#define PRIx16 hx
#define PRIx32 lx
#define PRIx64 llx

#define PRIX8 hhX
#define PRIX16 hX
#define PRIX32 lX
#define PRIX64 llX


#define PRId8LEAST hhd
#define PRId16LEAST hd
#define PRId32LEAST ld
#define PRId64LEAST lld

#define PRIi8LEAST PRId8LEAST
#define PRIi16LEAST PRId16LEAST
#define PRIi32LEAST PRId32LEAST
#define PRIi64LEAST PRId64LEAST

#define PRIu8LEAST hhu
#define PRIu16LEAST hu
#define PRIu32LEAST lu
#define PRIu64LEAST llu

#define PRIo8LEAST hho
#define PRIo16LEAST ho
#define PRIo32LEAST lo
#define PRIo64LEAST llo

#define PRIx8LEAST hhx
#define PRIx16LEAST hx
#define PRIx32LEAST lx
#define PRIx64LEAST llx

#define PRIX8LEAST hhX
#define PRIX16LEAST hX
#define PRIX32LEAST lX
#define PRIX64LEAST llX


#define PRId8FAST hhd
#define PRId16FAST hd
#define PRId32FAST ld
#define PRId64FAST lld

#define PRIi8FAST PRId8FAST
#define PRIi16FAST PRId16FAST
#define PRIi32FAST PRId32FAST
#define PRIi64FAST PRId64FAST

#define PRIu8FAST hhu
#define PRIu16FAST hu
#define PRIu32FAST lu
#define PRIu64FAST llu

#define PRIo8FAST hho
#define PRIo16FAST ho
#define PRIo32FAST lo
#define PRIo64FAST llo

#define PRIx8FAST hhx
#define PRIx16FAST hx
#define PRIx32FAST lx
#define PRIx64FAST llx

#define PRIX8FAST hhX
#define PRIX16FAST hX
#define PRIX32FAST lX
#define PRIX64FAST llX


#define PRIdMAX lld
#define PRIiMAX lld
#define PRIuMAX llu
#define PRIoMAX llo
#define PRIxMAX llx
#define PRIXMAX llX


#define PRIdPTR ld
#define PRIiPTR ld
#define PRIuPTR lu
#define PRIoPTR lo
#define PRIxPTR lx
#define PRIXPTR lX


#define SCNd8 hhd
#define SCNd16 hd
#define SCNd32 ld
#define SCNd64 lld

#define SCNi8 SCNd8
#define SCNi16 SCNd16
#define SCNi32 SCNd32
#define SCNi64 SCNd64

#define SCNu8 hhu
#define SCNu16 hu
#define SCNu32 lu
#define SCNu64 llu

#define SCNo8 hho
#define SCNo16 ho
#define SCNo32 lo
#define SCNo64 llo

#define SCNx8 hhx
#define SCNx16 hx
#define SCNx32 lx
#define SCNx64 llx


#define SCNd8LEAST hhd
#define SCNd16LEAST hd
#define SCNd32LEAST ld
#define SCNd64LEAST lld

#define SCNi8LEAST SCNd8LEAST
#define SCNi16LEAST SCNd16LEAST
#define SCNi32LEAST SCNd32LEAST
#define SCNi64LEAST SCNd64LEAST

#define SCNu8LEAST hhu
#define SCNu16LEAST hu
#define SCNu32LEAST lu
#define SCNu64LEAST llu

#define SCNo8LEAST hho
#define SCNo16LEAST ho
#define SCNo32LEAST lo
#define SCNo64LEAST llo

#define SCNx8LEAST hhx
#define SCNx16LEAST hx
#define SCNx32LEAST lx
#define SCNx64LEAST llx


#define SCNd8FAST hhd
#define SCNd16FAST hd
#define SCNd32FAST ld
#define SCNd64FAST lld

#define SCNi8FAST SCNd8FAST
#define SCNi16FAST SCNd16FAST
#define SCNi32FAST SCNd32FAST
#define SCNi64FAST SCNd64FAST

#define SCNu8FAST hhu
#define SCNu16FAST hu
#define SCNu32FAST lu
#define SCNu64FAST llu

#define SCNo8FAST hho
#define SCNo16FAST ho
#define SCNo32FAST lo
#define SCNo64FAST llo

#define SCNx8FAST hhx
#define SCNx16FAST hx
#define SCNx32FAST lx
#define SCNx64FAST llx


#define SCNdMAX lld
#define SCNiMAX lld
#define SCNuMAX llu
#define SCNoMAX llo
#define SCNxMAX llx
#define SCNXMAX llX


#define SCNdPTR ld
#define SCNiPTR ld
#define SCNuPTR lu
#define SCNoPTR lo
#define SCNxPTR lx
#define SCNXPTR lX

#endif /* _INTTYPES_H */
