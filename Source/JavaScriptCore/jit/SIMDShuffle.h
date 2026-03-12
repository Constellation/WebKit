/*
 * Copyright (C) 2023-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 the V8 project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/CPU.h>
#include <JavaScriptCore/SIMDInfo.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

// Canonical shuffle patterns recognized for ARM64 specialized instruction selection.
enum class CanonicalShuffle : uint8_t {
    Unknown,
    Identity,
    // 64-bit element patterns
    S64x2UnzipEven,                // UZP1.2D
    S64x2UnzipOdd,                 // UZP2.2D
    S64x2Reverse,                  // EXT #8 (unary): swap two 64-bit halves
    // 32-bit element patterns
    S32x4UnzipEven,                // UZP1.4S
    S32x4UnzipOdd,                 // UZP2.4S
    S32x4ZipLower,                 // ZIP1.4S
    S32x4ZipHigher,                // ZIP2.4S
    S32x4TransposeEven,            // TRN1.4S
    S32x4TransposeOdd,             // TRN2.4S
    S32x2Reverse,                  // REV64.4S
    // 16-bit element patterns
    S16x8UnzipEven,                // UZP1.8H
    S16x8UnzipOdd,                 // UZP2.8H
    S16x8ZipLower,                 // ZIP1.8H
    S16x8ZipHigher,                // ZIP2.8H
    S16x8TransposeEven,            // TRN1.8H
    S16x8TransposeOdd,             // TRN2.8H
    S16x2Reverse,                  // REV32.8H
    S16x4Reverse,                  // REV64.8H
    // 8-bit element patterns
    S8x16UnzipEven,                // UZP1.16B
    S8x16UnzipOdd,                 // UZP2.16B
    S8x16ZipLower,                 // ZIP1.16B
    S8x16ZipHigher,                // ZIP2.16B
    S8x16TransposeEven,            // TRN1.16B
    S8x16TransposeOdd,             // TRN2.16B
    S8x2Reverse,                   // REV16.16B
    S8x4Reverse,                   // REV32.16B
    S8x8Reverse,                   // REV64.16B
};

class SIMDShuffle {
public:
    static std::optional<unsigned> isOnlyOneSideMask(v128_t pattern)
    {
        unsigned first = pattern.u8x16[0];
        if (first < 16) {
            for (unsigned i = 1; i < 16; ++i) {
                if (pattern.u8x16[i] >= 16)
                    return std::nullopt;
            }
            return 0;
        }

        if (first >= 32)
            return std::nullopt;

        for (unsigned i = 1; i < 16; ++i) {
            if (pattern.u8x16[i] < 16)
                return std::nullopt;
            if (pattern.u8x16[i] >= 32)
                return std::nullopt;
        }
        return 1;
    }

    static std::optional<uint8_t> isI8x16DupElement(v128_t pattern)
    {
        constexpr unsigned numberOfElements = 16 / sizeof(uint8_t);
        if (std::all_of(pattern.u8x16, pattern.u8x16 + numberOfElements, [&](auto value) { return value == pattern.u8x16[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint8_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI16x8DupElement(v128_t pattern)
    {
        if (!isI16x8Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint16_t);
        if (std::all_of(pattern.u16x8, pattern.u16x8 + numberOfElements, [&](auto value) { return value == pattern.u16x8[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint16_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI32x4DupElement(v128_t pattern)
    {
        if (!isI32x4Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint32_t);
        if (std::all_of(pattern.u32x4, pattern.u32x4 + numberOfElements, [&](auto value) { return value == pattern.u32x4[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint32_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static std::optional<uint8_t> isI64x2DupElement(v128_t pattern)
    {
        if (!isI64x2Shuffle(pattern))
            return std::nullopt;
        constexpr unsigned numberOfElements = 16 / sizeof(uint64_t);
        if (std::all_of(pattern.u64x2, pattern.u64x2 + numberOfElements, [&](auto value) { return value == pattern.u64x2[0]; })) {
            uint8_t lane = pattern.u8x16[0] / sizeof(uint64_t);
            if (lane < numberOfElements)
                return lane;
        }
        return std::nullopt;
    }

    static bool isI16x8Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 2);
    }

    static bool isI32x4Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 4);
    }

    static bool isI64x2Shuffle(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 8);
    }

    static bool isIdentity(v128_t pattern)
    {
        return isLargerElementShuffle(pattern, 16);
    }

    static bool isAllOutOfBoundsForUnaryShuffle(v128_t pattern)
    {
        for (unsigned i = 0; i < 16; ++i) {
            if constexpr (isX86()) {
                // https://www.felixcloutier.com/x86/pshufb
                // On x64, OOB index means that highest bit is set.
                // The acutal index is extracted by masking with 0b1111.
                // So, for example, 0x11 index (17) will be converted to 0x1 access (not OOB).
                if (!(pattern.u8x16[i] & 0x80))
                    return false;
            } else if constexpr (isARM64()) {
                // https://developer.arm.com/documentation/dui0801/g/A64-SIMD-Vector-Instructions/TBL--vector-
                // On ARM64, OOB index means out of 0..15 range for unary TBL.
                if (pattern.u8x16[i] < 16)
                    return false;
            } else
                return false;
        }
        return true;
    }

    static bool isAllOutOfBoundsForBinaryShuffle(v128_t pattern)
    {
        ASSERT(isARM64()); // Binary Shuffle is only supported by ARM64.
        for (unsigned i = 0; i < 16; ++i) {
            if (pattern.u8x16[i] < 32)
                return false;
        }
        return true;
    }

    // Detect EXT (byte extraction / concatenation) pattern for binary shuffle.
    // Returns the byte offset if the pattern is {offset, offset+1, ..., 31, 0, 1, ...}
    // i.e., pattern[i] == (offset + i) % 32 for some offset in [0, 15].
    // ARM64 EXT instruction: EXT Vd.16B, Vn.16B, Vm.16B, #imm
    // extracts bytes from the concatenation of Vn:Vm.
    static std::optional<uint8_t> isEXT(v128_t pattern)
    {
        uint8_t offset = pattern.u8x16[0];
        if (offset >= 32)
            return std::nullopt;
        for (unsigned i = 1; i < 16; ++i) {
            if (pattern.u8x16[i] != ((offset + i) % 32))
                return std::nullopt;
        }
        // EXT #0 from (a, b) is identity of a, EXT #16+ doesn't exist.
        // But offset can be 0..31 as pattern indices.
        // ARM64 EXT supports imm in 0..15.
        if (offset < 16)
            return static_cast<uint8_t>(offset);
        // offset 16..31 means: EXT #(offset-16) with swapped operands.
        // The caller needs to handle swapping.
        return std::nullopt;
    }

    // Like isEXT, but also detect patterns where operands need swapping.
    // Returns {offset, needsSwap}.
    struct EXTInfo {
        uint8_t offset;
        bool needsSwap;
    };
    static std::optional<EXTInfo> isEXTWithSwap(v128_t pattern)
    {
        uint8_t first = pattern.u8x16[0];
        if (first >= 32)
            return std::nullopt;
        for (unsigned i = 1; i < 16; ++i) {
            if (pattern.u8x16[i] != ((first + i) % 32))
                return std::nullopt;
        }
        if (first < 16)
            return EXTInfo { first, false };
        return EXTInfo { static_cast<uint8_t>(first - 16), true };
    }

    // Try to match a canonical binary shuffle pattern for ARM64 specialized instructions.
    static CanonicalShuffle tryMatchCanonicalBinary(v128_t pattern)
    {
        // 64-bit element patterns (need i64x2-level shuffle)
        if (isI64x2BinaryShuffle(pattern)) {
            // UZP1.2D: even 64-bit elements = {child0[0..7], child1[0..7]}
            if (matchBinaryPattern64(pattern, 0, 2))
                return CanonicalShuffle::S64x2UnzipEven;
            // UZP2.2D: odd 64-bit elements = {child0[8..15], child1[8..15]}
            if (matchBinaryPattern64(pattern, 1, 2))
                return CanonicalShuffle::S64x2UnzipOdd;
        }

        // 32-bit element patterns
        if (isI32x4BinaryShuffle(pattern)) {
            // UZP1.4S: even 32-bit elements
            if (matchBinaryUnzip32(pattern, false))
                return CanonicalShuffle::S32x4UnzipEven;
            // UZP2.4S: odd 32-bit elements
            if (matchBinaryUnzip32(pattern, true))
                return CanonicalShuffle::S32x4UnzipOdd;
            // ZIP1.4S: interleave low halves
            if (matchBinaryZip32(pattern, false))
                return CanonicalShuffle::S32x4ZipLower;
            // ZIP2.4S: interleave high halves
            if (matchBinaryZip32(pattern, true))
                return CanonicalShuffle::S32x4ZipHigher;
            // TRN1.4S: transpose even
            if (matchBinaryTranspose32(pattern, false))
                return CanonicalShuffle::S32x4TransposeEven;
            // TRN2.4S: transpose odd
            if (matchBinaryTranspose32(pattern, true))
                return CanonicalShuffle::S32x4TransposeOdd;
        }

        // 16-bit element patterns
        if (isI16x8BinaryShuffle(pattern)) {
            if (matchBinaryUnzip16(pattern, false))
                return CanonicalShuffle::S16x8UnzipEven;
            if (matchBinaryUnzip16(pattern, true))
                return CanonicalShuffle::S16x8UnzipOdd;
            if (matchBinaryZip16(pattern, false))
                return CanonicalShuffle::S16x8ZipLower;
            if (matchBinaryZip16(pattern, true))
                return CanonicalShuffle::S16x8ZipHigher;
            if (matchBinaryTranspose16(pattern, false))
                return CanonicalShuffle::S16x8TransposeEven;
            if (matchBinaryTranspose16(pattern, true))
                return CanonicalShuffle::S16x8TransposeOdd;
        }

        // 8-bit element patterns
        {
            if (matchBinaryUnzip8(pattern, false))
                return CanonicalShuffle::S8x16UnzipEven;
            if (matchBinaryUnzip8(pattern, true))
                return CanonicalShuffle::S8x16UnzipOdd;
            if (matchBinaryZip8(pattern, false))
                return CanonicalShuffle::S8x16ZipLower;
            if (matchBinaryZip8(pattern, true))
                return CanonicalShuffle::S8x16ZipHigher;
            if (matchBinaryTranspose8(pattern, false))
                return CanonicalShuffle::S8x16TransposeEven;
            if (matchBinaryTranspose8(pattern, true))
                return CanonicalShuffle::S8x16TransposeOdd;
        }

        return CanonicalShuffle::Unknown;
    }

    // Try to match a unary shuffle pattern as a binary canonical shuffle.
    // For a unary shuffle (all indices 0..15), we construct a synthetic binary pattern
    // where the second half references "child1" (same register with both inputs).
    // E.g., unary {0,1,2,3,8,9,10,11, 0,1,2,3,8,9,10,11} becomes binary
    // {0,1,2,3,8,9,10,11, 16,17,18,19,24,25,26,27} which matches UZP1.4S.
    static CanonicalShuffle tryMatchUnaryAsBinaryCanonical(v128_t pattern)
    {
        // Construct synthetic binary pattern: second half indices += 16
        v128_t binaryPattern;
        for (unsigned i = 0; i < 8; ++i) {
            if (pattern.u8x16[i] >= 16)
                return CanonicalShuffle::Unknown; // Out of range for unary
            binaryPattern.u8x16[i] = pattern.u8x16[i];
        }
        for (unsigned i = 8; i < 16; ++i) {
            if (pattern.u8x16[i] >= 16)
                return CanonicalShuffle::Unknown; // Out of range for unary
            binaryPattern.u8x16[i] = pattern.u8x16[i] + 16;
        }
        return tryMatchCanonicalBinary(binaryPattern);
    }

    // Try to match a canonical unary shuffle pattern.
    static CanonicalShuffle tryMatchCanonicalUnary(v128_t pattern)
    {
        // REV64.4S: reverse pairs of 32-bit elements within each 64-bit lane
        // {4,5,6,7, 0,1,2,3, 12,13,14,15, 8,9,10,11}
        if (matchUnaryReverse(pattern, 4, 8))
            return CanonicalShuffle::S32x2Reverse;

        // REV64.8H: reverse 16-bit elements within each 64-bit lane
        if (matchUnaryReverse(pattern, 2, 8))
            return CanonicalShuffle::S16x4Reverse;

        // REV32.8H: reverse pairs of 16-bit elements within each 32-bit lane
        if (matchUnaryReverse(pattern, 2, 4))
            return CanonicalShuffle::S16x2Reverse;

        // REV64.16B: reverse bytes within each 64-bit lane
        if (matchUnaryReverse(pattern, 1, 8))
            return CanonicalShuffle::S8x8Reverse;

        // REV32.16B: reverse bytes within each 32-bit lane
        if (matchUnaryReverse(pattern, 1, 4))
            return CanonicalShuffle::S8x4Reverse;

        // REV16.16B: reverse bytes within each 16-bit lane
        if (matchUnaryReverse(pattern, 1, 2))
            return CanonicalShuffle::S8x2Reverse;

        // S64x2 reverse: swap two 64-bit halves = {8..15, 0..7}
        if (isI64x2Shuffle(pattern) && pattern.u8x16[0] == 8)
            return CanonicalShuffle::S64x2Reverse;

        return CanonicalShuffle::Unknown;
    }

    // Compose an outer shuffle with an inner shuffle.
    // Given outer = shuffle(inner_result, other, outerPattern) or
    //       outer = shuffle(other, inner_result, outerPattern)
    // where inner_result = shuffle(innerSrc, innerPattern) [unary]
    // Produces a combined pattern that reads from (innerSrc, other) or (other, innerSrc).
    //
    // innerIsChild0: whether the inner shuffle's result is child(0) of the outer.
    // Returns nullopt if composition is not possible (e.g., indices go out of range).
    static std::optional<v128_t> composeShuffle(v128_t outerPattern, v128_t innerPattern, bool innerIsChild0)
    {
        v128_t result;
        for (unsigned i = 0; i < 16; ++i) {
            uint8_t outerIdx = outerPattern.u8x16[i];
            if (outerIdx >= 32) {
                // Out of bounds in outer — result is 0 on ARM64 TBL.
                result.u8x16[i] = 0xFF; // OOB
                continue;
            }

            if (innerIsChild0) {
                if (outerIdx < 16) {
                    // Reads from inner's output. Chase through inner.
                    uint8_t innerIdx = innerPattern.u8x16[outerIdx];
                    if (innerIdx >= 16) {
                        result.u8x16[i] = 0xFF; // OOB in inner → zero
                        continue;
                    }
                    // innerIdx is 0..15, referring to inner's input.
                    // In the composed shuffle, inner's input is child0, other is child1.
                    result.u8x16[i] = innerIdx;
                } else {
                    // Reads from the other child (child1 of outer).
                    // In composed result, other is child1, offset 16..31.
                    result.u8x16[i] = outerIdx; // stays as 16..31
                }
            } else {
                // inner is child1 of outer
                if (outerIdx >= 16) {
                    // Reads from inner's output (which is child1 of outer).
                    uint8_t innerIdx = innerPattern.u8x16[outerIdx - 16];
                    if (innerIdx >= 16) {
                        result.u8x16[i] = 0xFF;
                        continue;
                    }
                    // inner's input becomes child1 in composed result.
                    result.u8x16[i] = innerIdx + 16;
                } else {
                    // Reads from the other child (child0 of outer).
                    result.u8x16[i] = outerIdx; // stays as 0..15
                }
            }
        }
        return result;
    }

    // Compute demanded byte mask for a shuffle pattern.
    // Returns a bitmask where bit i is set if byte i of the shuffle output is needed.
    // demandedOutputMask is the set of output bytes the consumer needs.
    static uint16_t computeDemandedInputBytesForUnary(v128_t pattern, uint16_t demandedOutputMask)
    {
        uint16_t demandedInput = 0;
        for (unsigned i = 0; i < 16; ++i) {
            if (!(demandedOutputMask & (1 << i)))
                continue;
            uint8_t idx = pattern.u8x16[i];
            if (idx < 16)
                demandedInput |= (1 << idx);
        }
        return demandedInput;
    }

    // Truncate a 16-byte shuffle pattern to an 8-byte pattern for VectorSwizzle8.
    // Only the low 8 bytes of the output are kept.
    static void truncatePatternTo8(v128_t& pattern)
    {
        // Zero out the high 8 bytes (they won't be used).
        for (unsigned i = 8; i < 16; ++i)
            pattern.u8x16[i] = 0xFF; // OOB index → zero output
    }

private:
    static bool isLargerElementShuffle(v128_t pattern, unsigned size)
    {
        unsigned numberOfElements = 16 / size;
        for (unsigned i = 0; i < numberOfElements; ++i) {
            unsigned firstIndex = i * size;
            unsigned first = pattern.u8x16[firstIndex];
            if (first % size != 0)
                return false;
            for (unsigned j = 1; j < size; ++j) {
                unsigned index = j + firstIndex;
                if (pattern.u8x16[index] != (first + j))
                    return false;
            }
        }
        return true;
    }

    // Check if pattern is a valid binary shuffle at 64-bit element granularity.
    static bool isI64x2BinaryShuffle(v128_t pattern)
    {
        // Each 8-byte group must be consecutive and start at a multiple of 8.
        for (unsigned elem = 0; elem < 2; ++elem) {
            unsigned base = elem * 8;
            unsigned first = pattern.u8x16[base];
            if (first % 8 != 0)
                return false;
            if (first >= 32)
                return false;
            for (unsigned j = 1; j < 8; ++j) {
                if (pattern.u8x16[base + j] != first + j)
                    return false;
            }
        }
        return true;
    }

    // Check if pattern is a valid binary shuffle at 32-bit element granularity.
    static bool isI32x4BinaryShuffle(v128_t pattern)
    {
        for (unsigned elem = 0; elem < 4; ++elem) {
            unsigned base = elem * 4;
            unsigned first = pattern.u8x16[base];
            if (first % 4 != 0)
                return false;
            if (first >= 32)
                return false;
            for (unsigned j = 1; j < 4; ++j) {
                if (pattern.u8x16[base + j] != first + j)
                    return false;
            }
        }
        return true;
    }

    // Check if pattern is a valid binary shuffle at 16-bit element granularity.
    static bool isI16x8BinaryShuffle(v128_t pattern)
    {
        for (unsigned elem = 0; elem < 8; ++elem) {
            unsigned base = elem * 2;
            unsigned first = pattern.u8x16[base];
            if (first % 2 != 0)
                return false;
            if (first >= 32)
                return false;
            if (pattern.u8x16[base + 1] != first + 1)
                return false;
        }
        return true;
    }

    // Match binary 64-bit even/odd pattern.
    // even: {child0[elem0], child1[elem0]} odd: {child0[elem1], child1[elem1]}
    static bool matchBinaryPattern64(v128_t pattern, unsigned startElem, unsigned /* count */)
    {
        // UZP1.2D (even): byte[0..7] = child0[0..7], byte[8..15] = child1[0..7]
        //   i.e., indices {0,1,...,7, 16,17,...,23}
        // UZP2.2D (odd): byte[0..7] = child0[8..15], byte[8..15] = child1[8..15]
        //   i.e., indices {8,9,...,15, 24,25,...,31}
        unsigned child0Start = startElem * 8;
        unsigned child1Start = 16 + startElem * 8;
        for (unsigned i = 0; i < 8; ++i) {
            if (pattern.u8x16[i] != child0Start + i)
                return false;
        }
        for (unsigned i = 0; i < 8; ++i) {
            if (pattern.u8x16[8 + i] != child1Start + i)
                return false;
        }
        return true;
    }

    // UZP for 32-bit elements: even picks elements 0,2 from each, odd picks 1,3.
    static bool matchBinaryUnzip32(v128_t pattern, bool odd)
    {
        // UZP1.4S (even): {a[0], a[2], b[0], b[2]} in 32-bit elements
        //   bytes: {a[0..3], a[8..11], b[0..3], b[8..11]}
        //   = {0,1,2,3, 8,9,10,11, 16,17,18,19, 24,25,26,27}
        // UZP2.4S (odd): {a[1], a[3], b[1], b[3]}
        //   = {4,5,6,7, 12,13,14,15, 20,21,22,23, 28,29,30,31}
        unsigned offset = odd ? 4 : 0;
        unsigned expected[4] = {
            0 + offset, 8 + offset, 16 + offset, 24 + offset
        };
        for (unsigned elem = 0; elem < 4; ++elem) {
            unsigned base = elem * 4;
            for (unsigned j = 0; j < 4; ++j) {
                if (pattern.u8x16[base + j] != expected[elem] + j)
                    return false;
            }
        }
        return true;
    }

    // ZIP for 32-bit elements.
    static bool matchBinaryZip32(v128_t pattern, bool high)
    {
        // ZIP1.4S (low): {a[0], b[0], a[1], b[1]} in 32-bit elements
        //   = {0,1,2,3, 16,17,18,19, 4,5,6,7, 20,21,22,23}
        // ZIP2.4S (high): {a[2], b[2], a[3], b[3]}
        //   = {8,9,10,11, 24,25,26,27, 12,13,14,15, 28,29,30,31}
        unsigned aStart = high ? 8 : 0;
        unsigned bStart = high ? 24 : 16;
        unsigned expected[4] = { aStart, bStart, aStart + 4, bStart + 4 };
        for (unsigned elem = 0; elem < 4; ++elem) {
            unsigned base = elem * 4;
            for (unsigned j = 0; j < 4; ++j) {
                if (pattern.u8x16[base + j] != expected[elem] + j)
                    return false;
            }
        }
        return true;
    }

    // TRN for 32-bit elements.
    static bool matchBinaryTranspose32(v128_t pattern, bool odd)
    {
        // TRN1.4S (even): {a[0], b[0], a[2], b[2]}
        //   = {0,1,2,3, 16,17,18,19, 8,9,10,11, 24,25,26,27}
        // TRN2.4S (odd): {a[1], b[1], a[3], b[3]}
        //   = {4,5,6,7, 20,21,22,23, 12,13,14,15, 28,29,30,31}
        unsigned offset = odd ? 4 : 0;
        unsigned expected[4] = { 0 + offset, 16 + offset, 8 + offset, 24 + offset };
        for (unsigned elem = 0; elem < 4; ++elem) {
            unsigned base = elem * 4;
            for (unsigned j = 0; j < 4; ++j) {
                if (pattern.u8x16[base + j] != expected[elem] + j)
                    return false;
            }
        }
        return true;
    }

    // UZP for 16-bit elements.
    static bool matchBinaryUnzip16(v128_t pattern, bool odd)
    {
        // UZP1.8H (even): {a[0],a[2],a[4],a[6], b[0],b[2],b[4],b[6]} in 16-bit elements
        // UZP2.8H (odd): {a[1],a[3],a[5],a[7], b[1],b[3],b[5],b[7]}
        unsigned offset = odd ? 2 : 0;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned srcChild = (i < 4) ? 0 : 16;
            unsigned elemIdx = (i % 4) * 4 + offset; // byte offset within child
            unsigned expected = srcChild + elemIdx;
            unsigned base = i * 2;
            if (pattern.u8x16[base] != expected || pattern.u8x16[base + 1] != expected + 1)
                return false;
        }
        return true;
    }

    // ZIP for 16-bit elements.
    static bool matchBinaryZip16(v128_t pattern, bool high)
    {
        // ZIP1.8H (low): {a[0],b[0],a[1],b[1],a[2],b[2],a[3],b[3]}
        // ZIP2.8H (high): {a[4],b[4],a[5],b[5],a[6],b[6],a[7],b[7]}
        unsigned startElem = high ? 4 : 0;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned childSrc = (i & 1) ? 16 : 0;
            unsigned elemIdx = startElem + (i >> 1);
            unsigned expected = childSrc + elemIdx * 2;
            unsigned base = i * 2;
            if (pattern.u8x16[base] != expected || pattern.u8x16[base + 1] != expected + 1)
                return false;
        }
        return true;
    }

    // TRN for 16-bit elements.
    static bool matchBinaryTranspose16(v128_t pattern, bool odd)
    {
        // TRN1.8H: {a[0],b[0],a[2],b[2],a[4],b[4],a[6],b[6]}
        // TRN2.8H: {a[1],b[1],a[3],b[3],a[5],b[5],a[7],b[7]}
        unsigned offset = odd ? 1 : 0;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned childSrc = (i & 1) ? 16 : 0;
            unsigned elemIdx = (i & ~1u) + offset; // pair index * 2 + offset
            unsigned expected = childSrc + elemIdx * 2;
            unsigned base = i * 2;
            if (pattern.u8x16[base] != expected || pattern.u8x16[base + 1] != expected + 1)
                return false;
        }
        return true;
    }

    // UZP for 8-bit elements.
    static bool matchBinaryUnzip8(v128_t pattern, bool odd)
    {
        // UZP1.16B (even): {a[0],a[2],...,a[14], b[0],b[2],...,b[14]}
        // UZP2.16B (odd): {a[1],a[3],...,a[15], b[1],b[3],...,b[15]}
        unsigned offset = odd ? 1 : 0;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned childSrc = (i < 8) ? 0 : 16;
            unsigned expected = childSrc + (i % 8) * 2 + offset;
            if (pattern.u8x16[i] != expected)
                return false;
        }
        return true;
    }

    // ZIP for 8-bit elements.
    static bool matchBinaryZip8(v128_t pattern, bool high)
    {
        // ZIP1.16B (low): {a[0],b[0],a[1],b[1],...,a[7],b[7]}
        // ZIP2.16B (high): {a[8],b[8],a[9],b[9],...,a[15],b[15]}
        unsigned startElem = high ? 8 : 0;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned childSrc = (i & 1) ? 16 : 0;
            unsigned expected = childSrc + startElem + (i >> 1);
            if (pattern.u8x16[i] != expected)
                return false;
        }
        return true;
    }

    // TRN for 8-bit elements.
    static bool matchBinaryTranspose8(v128_t pattern, bool odd)
    {
        // TRN1.16B: {a[0],b[0],a[2],b[2],...,a[14],b[14]}
        // TRN2.16B: {a[1],b[1],a[3],b[3],...,a[15],b[15]}
        unsigned offset = odd ? 1 : 0;
        for (unsigned i = 0; i < 16; ++i) {
            unsigned childSrc = (i & 1) ? 16 : 0;
            unsigned expected = childSrc + (i & ~1u) + offset;
            if (pattern.u8x16[i] != expected)
                return false;
        }
        return true;
    }

    // Match unary reverse pattern: reverse elements of size `elemSize` within groups of `groupSize`.
    static bool matchUnaryReverse(v128_t pattern, unsigned elemSize, unsigned groupSize)
    {
        unsigned elemsPerGroup = groupSize / elemSize;
        unsigned numGroups = 16 / groupSize;
        for (unsigned group = 0; group < numGroups; ++group) {
            for (unsigned elem = 0; elem < elemsPerGroup; ++elem) {
                unsigned srcElem = elemsPerGroup - 1 - elem;
                unsigned dstBase = group * groupSize + elem * elemSize;
                unsigned srcBase = group * groupSize + srcElem * elemSize;
                for (unsigned j = 0; j < elemSize; ++j) {
                    if (pattern.u8x16[dstBase + j] != srcBase + j)
                        return false;
                }
            }
        }
        return true;
    }
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
