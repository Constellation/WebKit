/*
 * rapidhash - Very fast, high quality, platform-independent hashing algorithm.
 * Copyright (C) 2024 Nicolas De Carli
 *
 * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
 *
 * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You can contact the author at:
 *   - rapidhash source repository: https://github.com/Nicoshev/rapidhash
 */

#pragma once

#include <wtf/FastMalloc.h>
#include <wtf/Int128.h>
#include <wtf/UnalignedAccess.h>
#include <wtf/text/StringHasher.h>

#if CPU(ARM64)
#include <arm_neon.h>
#endif

#if CPU(X86_64)
#include <emmintrin.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// https://github.com/Nicoshev/rapidhash
class RapidHash {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(RapidHash);
public:
    static constexpr unsigned flagCount = StringHasher::flagCount;
    static constexpr unsigned maskHash = StringHasher::maskHash;
    using DefaultConverter = StringHasher::DefaultConverter;

    static constexpr std::array<uint64_t, 3> secret { 0x2d358dccaa6c78a5ull, 0x8bb84b93962eacc9ull, 0x4b33a62ed433d4a3ull };

    RapidHash() = default;

    template<typename T, typename Converter = DefaultConverter>
    ALWAYS_INLINE static constexpr unsigned computeHashAndMaskTop8Bits(std::span<const T> data)
    {
        return StringHasher::avoidZero(static_cast<unsigned>(rapidhash<T, Converter>(data)) & StringHasher::maskHash);
    }

private:
    friend class StringHasher;

    ALWAYS_INLINE static constexpr std::pair<uint64_t, uint64_t> rapidMul128(uint64_t A, uint64_t B)
    {
        UInt128 r = static_cast<UInt128>(A) * B;
        return { static_cast<uint64_t>(r), static_cast<uint64_t>(r >> 64) };
    }

    ALWAYS_INLINE static constexpr uint64_t rapidMix(uint64_t A, uint64_t B)
    {
        auto [lo, hi] = rapidMul128(A, B);
        return lo ^ hi;
    }

    // Core rapidhash algorithm using index-based read functions.
    // Read64Fn: (size_t byteOffset) -> uint64_t  (reads 8 bytes at offset)
    // Read32Fn: (size_t byteOffset) -> uint64_t  (reads 4 bytes at offset)
    // ReadSmallFn: (size_t byteOffset, size_t k) -> uint64_t  (reads 1-3 bytes)
    template<typename Read64Fn, typename Read32Fn, typename ReadSmallFn>
    ALWAYS_INLINE static constexpr uint64_t rapidhashImpl(size_t len, NOESCAPE const Read64Fn& read64, NOESCAPE const Read32Fn& read32, NOESCAPE const ReadSmallFn& readSmall)
    {
        uint64_t seed = rapidMix(0 ^ secret[0], secret[1]) ^ len;
        uint64_t a, b;

        if (len <= 16) [[likely]] {
            if (len >= 4) [[likely]] {
                const uint64_t delta = (len >= 8) ? 4 : 0;
                a = (read32(0) << 32) | read32(len - 4);
                b = (read32(delta) << 32) | read32(len - 4 - delta);
            } else if (len > 0) [[likely]] {
                a = readSmall(0, len);
                b = 0;
            } else {
                a = 0;
                b = 0;
            }
        } else {
            size_t i = len;
            size_t off = 0;
            if (i > 48) [[unlikely]] {
                uint64_t see1 = seed, see2 = seed;
                do {
                    seed = rapidMix(read64(off) ^ secret[0], read64(off + 8) ^ seed);
                    see1 = rapidMix(read64(off + 16) ^ secret[1], read64(off + 24) ^ see1);
                    see2 = rapidMix(read64(off + 32) ^ secret[2], read64(off + 40) ^ see2);
                    off += 48;
                    i -= 48;
                } while (i >= 48);
                seed ^= see1 ^ see2;
            }
            if (i > 16) {
                seed = rapidMix(read64(off) ^ secret[2], read64(off + 8) ^ seed ^ secret[1]);
                if (i > 32)
                    seed = rapidMix(read64(off + 16) ^ secret[2], read64(off + 24) ^ seed);
            }

            a = read64(off + i - 16);
            b = read64(off + i - 8);
        }

        a ^= secret[1];
        b ^= seed;
        auto [aLo, aHi] = rapidMul128(a, b);
        return rapidMix(aLo ^ secret[0] ^ len, aHi ^ secret[1]);
    }

    // Hash one byte per input character. The byte produced for each character is
    // (Converter::convert(c) & 0xFF) | (Converter::convert(c) >> 8). For Latin1
    // characters with DefaultConverter this is just the byte; for char16_t with
    // DefaultConverter this is the OR-fold (low | high). The same byte stream
    // (and same length N) is produced for the same content regardless of input
    // width, so hash("ABC" Latin1) == hash(u"ABC"), and the same converter on
    // 8-bit and 16-bit inputs produces matching hashes.
    //
    // Fast paths (runtime only) for DefaultConverter:
    //   - Latin1: unalignedLoad — bytes match output bytes directly.
    //   - char16_t: SIMD OR-fold (NEON / SSE2).
    // All other cases (custom Converter, or constexpr evaluation) use a scalar
    // per-char fold loop, identical bytes guaranteed.
    template<typename T, typename Converter>
    ALWAYS_INLINE static constexpr uint64_t rapidhash(std::span<const T> data)
    {
        const T* p = data.data();

        auto foldByte = [&](size_t idx) ALWAYS_INLINE_LAMBDA -> uint8_t {
            if constexpr (std::is_same_v<Converter, DefaultConverter>) {
                if constexpr (sizeof(T) == 1)
                    return static_cast<uint8_t>(data[idx]);
                else
                    return static_cast<uint8_t>((data[idx] & 0xFF) | (data[idx] >> 8));
            } else {
                auto conv = Converter::convert(data[idx]);
                return static_cast<uint8_t>((conv & 0xFF) | (conv >> 8));
            }
        };

        auto read64 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
#if CPU(LITTLE_ENDIAN)
            if (!std::is_constant_evaluated()) {
                if constexpr (std::is_same_v<Converter, DefaultConverter>) {
                    if constexpr (sizeof(T) == 1)
                        return unalignedLoad<uint64_t>(p + off);
                    else if constexpr (sizeof(T) == 2) {
#if CPU(ARM64)
                        uint16x8_t x;
                        memcpy(&x, p + off, sizeof(x));
                        uint16x8_t folded = vorrq_u16(x, vshrq_n_u16(x, 8));
                        return vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(folded)), 0);
#elif CPU(X86_64)
                        __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + off));
                        __m128i folded = _mm_or_si128(x, _mm_srli_epi16(x, 8));
                        return _mm_cvtsi128_si64(_mm_packus_epi16(folded, _mm_setzero_si128()));
#endif
                    }
                }
            }
#endif
            uint64_t result = 0;
            for (size_t i = 0; i < 8; ++i)
                result |= static_cast<uint64_t>(foldByte(off + i)) << (i * 8);
            return result;
        };

        auto read32 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
#if CPU(LITTLE_ENDIAN)
            if (!std::is_constant_evaluated()) {
                if constexpr (std::is_same_v<Converter, DefaultConverter>) {
                    if constexpr (sizeof(T) == 1)
                        return static_cast<uint64_t>(unalignedLoad<uint32_t>(p + off));
                    else if constexpr (sizeof(T) == 2) {
#if CPU(ARM64)
                        uint16x4_t x;
                        memcpy(&x, p + off, sizeof(x));
                        uint16x8_t xWide = vcombine_u16(x, x);
                        uint16x8_t folded = vorrq_u16(xWide, vshrq_n_u16(xWide, 8));
                        return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(folded)), 0);
#elif CPU(X86_64)
                        __m128i x = _mm_loadu_si64(reinterpret_cast<const __m128i*>(p + off));
                        __m128i folded = _mm_or_si128(x, _mm_srli_epi16(x, 8));
                        return _mm_cvtsi128_si64(_mm_packus_epi16(folded, _mm_setzero_si128()));
#endif
                    }
                }
            }
#endif
            uint64_t result = 0;
            for (size_t i = 0; i < 4; ++i)
                result |= static_cast<uint64_t>(foldByte(off + i)) << (i * 8);
            return result;
        };

        auto readSmall = [&](size_t off, size_t k) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return (static_cast<uint64_t>(foldByte(off)) << 56)
                | (static_cast<uint64_t>(foldByte(off + (k >> 1))) << 32)
                | static_cast<uint64_t>(foldByte(off + k - 1));
        };

        return rapidhashImpl(data.size(), read64, read32, readSmall);
    }

};

} // namespace WTF

using WTF::RapidHash;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
