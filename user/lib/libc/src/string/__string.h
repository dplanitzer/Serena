//
//  __string.h
//  libc, libsc
//
//  Created by Dietmar Planitzer on 12/30/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ___STRING_H
#define ___STRING_H 1

#include <stdint.h>
#include <string.h>

#if defined(__ILP32__)
typedef uint32_t uword_t;
#define WORD_SIZE       4
#define WORD_SIZMASK    3
#define WORD_SHIFT      2
#define HALFWORD_FROM_BYTE(b) (((b) << 8) | (b))
#define WORD_FROM_BYTE(b) ((HALFWORD_FROM_BYTE(b) << 16) | HALFWORD_FROM_BYTE(b))
#elif defined(__LLP64__) || defined(__LP64__)
typedef uint64_t uword_t;
#define WORD_SIZE       8
#define WORD_SIZMASK    7
#define WORD_SHIFT      3
#define WORD_FROM_BYTE(b) ((b) << 56) | ((b) << 48) | ((b) << 40) | ((b) << 32) | ((b) << 24) | ((b) << 16) | ((b) << 8) | (b)
#else
#error "unknown data model"
#endif

#if defined(__M68K__) || defined(__x86_64__) || defined(__i386__)
// CPU just requires that a word/long word move is done with a 16bit aligned address
#define __SIMPLE_ALIGNMENT_MODEL 1
#endif

#endif /* ___STRING_H */
