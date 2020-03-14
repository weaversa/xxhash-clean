/*
 *  xxHash - Fast Hash algorithm
 *  Copyright (C) 2012-2020 Yann Collet
 *  Copyright (C) 2019-2020 Devin Hussey (easyaspi314)
 *
 *  BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *  copyright notice, this list of conditions and the following disclaimer
 *  in the documentation and/or other materials provided with the
 *  distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You can contact the author at :
 *  - xxHash homepage: http://www.xxhash.com
 *  - xxHash source repository : https://github.com/Cyan4973/xxHash
 */

/* This is a compact, 100% standalone reference XXH64 single-run implementation.
 * Instead of focusing on performance hacks, this focuses on cleanliness,
 * conformance, portability and simplicity.
 *
 * This file aims to be 100% compatible with C90/C++98, with the additional
 * requirement of stdint.h and long long. No library functions are used. */

#include <stddef.h> /* size_t, NULL */
#include <stdint.h> /* uint8_t, uint32_t, uint64_t */

#ifdef __cplusplus
extern "C"
#endif
uint64_t XXH64(void const *const input, size_t const length, uint64_t const seed);

static uint64_t const PRIME64_1 = 0x9E3779B185EBCA87ULL;   /* 0b1001111000110111011110011011000110000101111010111100101010000111 */
static uint64_t const PRIME64_2 = 0xC2B2AE3D27D4EB4FULL;   /* 0b1100001010110010101011100011110100100111110101001110101101001111 */
static uint64_t const PRIME64_3 = 0x165667B19E3779F9ULL;   /* 0b0001011001010110011001111011000110011110001101110111100111111001 */
static uint64_t const PRIME64_4 = 0x85EBCA77C2B2AE63ULL;   /* 0b1000010111101011110010100111011111000010101100101010111001100011 */
static uint64_t const PRIME64_5 = 0x27D4EB2F165667C5ULL;   /* 0b0010011111010100111010110010111100010110010101100110011111000101 */

/* Rotates value left by amount bits. */
static uint64_t XXH_rotl64(uint64_t const value, uint32_t const amount)
{
    return (value << amount) | (value >> (64 - amount));
}

/* Portably reads a 32-bit little endian integer from data at the given offset. */
static uint32_t XXH_read32(uint8_t const *const data, size_t const offset)
{
    return (uint32_t) data[offset + 0]
        | ((uint32_t) data[offset + 1] <<  8)
        | ((uint32_t) data[offset + 2] << 16)
        | ((uint32_t) data[offset + 3] << 24);
}

/* Portably reads a 64-bit little endian integer from data at the given offset. */
static uint64_t XXH_read64(uint8_t const *const data, size_t const offset)
{
    return (uint64_t) data[offset + 0]
        | ((uint64_t) data[offset + 1] <<  8)
        | ((uint64_t) data[offset + 2] << 16)
        | ((uint64_t) data[offset + 3] << 24)
        | ((uint64_t) data[offset + 4] << 32)
        | ((uint64_t) data[offset + 5] << 40)
        | ((uint64_t) data[offset + 6] << 48)
        | ((uint64_t) data[offset + 7] << 56);
}

/* Mixes input into lane, this is mostly used in the first loop. */
static uint64_t XXH64_round(uint64_t lane, uint64_t const input)
{
    lane += input * PRIME64_2;
    lane  = XXH_rotl64(lane, 31);
    lane *= PRIME64_1;
    return lane;
}

/* Merges lane into hash to finalize */
static uint64_t XXH64_mergeRound(uint64_t hash, uint64_t const lane)
{
    hash ^= XXH64_round(0, lane);
    hash *= PRIME64_1;
    hash += PRIME64_4;
    return hash;
}

/* Mixes all bits to finalize the hash. */
static uint64_t XXH64_avalanche(uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= PRIME64_2;
    hash ^= hash >> 29;
    hash *= PRIME64_3;
    hash ^= hash >> 32;
    return hash;
}

/* The XXH64 hash function.
 * input:   The data to hash.
 * length:  The length of input. It is undefined behavior to have length larger than the
 *          capacity of input.
 * seed:    A 64-bit value to seed the hash with.
 * returns: The 64-bit calculated hash value. */
uint64_t XXH64(void const *const input, size_t const length, uint64_t const seed)
{
    uint8_t const *const data = (uint8_t const *) input;
    uint64_t hash = 0;
    size_t remaining = length;
    size_t offset = 0;

    /* Don't dereference a null pointer. The reference implementation notably doesn't
     * check for this by default. */
    if (input == NULL) {
        return XXH64_avalanche(seed + PRIME64_5);
    }

    if (remaining >= 32) {
        /* Initialize our lanes */
        uint64_t lane1 = seed + PRIME64_1 + PRIME64_2;
        uint64_t lane2 = seed + PRIME64_2;
        uint64_t lane3 = seed + 0;
        uint64_t lane4 = seed - PRIME64_1;

        while (remaining >= 32) {
            lane1 = XXH64_round(lane1, XXH_read64(data, offset)); offset += 8;
            lane2 = XXH64_round(lane2, XXH_read64(data, offset)); offset += 8;
            lane3 = XXH64_round(lane3, XXH_read64(data, offset)); offset += 8;
            lane4 = XXH64_round(lane4, XXH_read64(data, offset)); offset += 8;
            remaining -= 32;
        }

        hash = XXH_rotl64(lane1, 1) + XXH_rotl64(lane2, 7) + XXH_rotl64(lane3, 12) + XXH_rotl64(lane4, 18);

        hash = XXH64_mergeRound(hash, lane1);
        hash = XXH64_mergeRound(hash, lane2);
        hash = XXH64_mergeRound(hash, lane3);
        hash = XXH64_mergeRound(hash, lane4);
    } else {
        /* Not enough data for the main loop, put something in there instead. */
        hash = seed + PRIME64_5;
    }

    hash += (uint64_t) length;

    /* Process the remaining data. */
    while (remaining >= 8) {
        hash ^= XXH64_round(0, XXH_read64(data, offset));
        hash  = XXH_rotl64(hash, 27);
        hash *= PRIME64_1;
        hash += PRIME64_4;
        offset += 8;
        remaining -= 8;
    }

    if (remaining >= 4) {
        hash ^= (uint64_t) XXH_read32(data, offset) * PRIME64_1;
        hash  = XXH_rotl64(hash, 23);
        hash *= PRIME64_2;
        hash += PRIME64_3;
        offset += 4;
        remaining -= 4;
    }

    while (remaining != 0) {
        hash ^= (uint64_t) data[offset] * PRIME64_5;
        hash  = XXH_rotl64(hash, 11);
        hash *= PRIME64_1;
        ++offset;
        --remaining;
    }

    return XXH64_avalanche(hash);
}

#ifdef XXH_SELFTEST
#include <stdio.h>  /* fprintf, puts */
#include <stdlib.h> /* exit */

#define TEST_DATA_SIZE 101
static int test_num = 0;
static uint32_t const PRIME32_1 = 0x9E3779B1U; /* 0b10011110001101110111100110110001 */

/* Checks a hash value. */
static void test_sequence(uint8_t const *const test_data, size_t const length,
                          uint64_t const seed, uint64_t const expected)
{
    uint64_t const result = XXH64(test_data, length, seed);
    if (result != expected) {
        fprintf(stderr, "Error: Test %i: XXH64 test failed!\n", ++test_num);
        fprintf(stderr, "\rExpected value: 0x%08X%08X. Actual value: 0x%08X%08X.\n",
                (uint32_t) (expected >> 32), (uint32_t) expected, (uint32_t) (result >> 32), (uint32_t) result);
        exit(1);
    }
}


int main(void)
{
    uint32_t const prime = PRIME32_1;
    uint8_t test_data[TEST_DATA_SIZE];
    uint32_t byte_gen = prime;
    size_t i = 0;

    /* Fill in the test_data buffer with "random" data. */
    for (; i < TEST_DATA_SIZE; ++i) {
        test_data[i] = (uint8_t) (byte_gen >> 24);
        byte_gen *= byte_gen;
    }

    test_sequence(NULL     ,  0            , 0    , 0xEF46DB3751D8E999ULL);
    test_sequence(NULL     ,  0            , prime, 0xAC75FDA2929B17EFULL);
    test_sequence(test_data,  1            , 0    , 0x4FCE394CC88952D8ULL);
    test_sequence(test_data,  1            , prime, 0x739840CB819FA723ULL);
    test_sequence(test_data, 14            , 0    , 0xCFFA8DB881BC3A3DULL);
    test_sequence(test_data, 14            , prime, 0x5B9611585EFCC9CBULL);
    test_sequence(test_data, TEST_DATA_SIZE, 0    , 0x0EAB543384F878ADULL);
    test_sequence(test_data, TEST_DATA_SIZE, prime, 0xCAA65939306F1E21ULL);

    puts("XXH64 reference implementation: OK");

    return 0;
}

#endif /* XXH_SELFTEST */
