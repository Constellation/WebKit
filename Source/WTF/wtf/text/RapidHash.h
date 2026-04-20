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
        return StringHasher::avoidZero(computeHashImpl<T, Converter>(data) & StringHasher::maskHash);
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
    ALWAYS_INLINE static constexpr uint64_t rapidhashCore(size_t len, NOESCAPE const Read64Fn& read64, NOESCAPE const Read32Fn& read32, NOESCAPE const ReadSmallFn& readSmall)
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

    // Hash raw bytes from a typed span. No reinterpret_cast, works in constexpr.
    template<typename T>
    ALWAYS_INLINE static constexpr uint64_t rapidhashBytes(std::span<const T> data)
    {
        static_assert(sizeof(T) == 1);
        auto read64 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return static_cast<uint64_t>(static_cast<uint8_t>(data[off]))
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 1])) << 8)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 2])) << 16)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 3])) << 24)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 4])) << 32)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 5])) << 40)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 6])) << 48)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 7])) << 56);
        };
        auto read32 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return static_cast<uint64_t>(static_cast<uint8_t>(data[off]))
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 1])) << 8)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 2])) << 16)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + 3])) << 24);
        };
        auto readSmall = [&](size_t off, size_t k) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return (static_cast<uint64_t>(static_cast<uint8_t>(data[off])) << 56)
                | (static_cast<uint64_t>(static_cast<uint8_t>(data[off + (k >> 1)])) << 32)
                | static_cast<uint64_t>(static_cast<uint8_t>(data[off + k - 1]));
        };
        return rapidhashCore(data.size(), read64, read32, readSmall);
    }

    // Runtime-only: hash raw bytes using unaligned loads.
    ALWAYS_INLINE static uint64_t rapidhashRawBytes(const uint8_t* p, size_t len)
    {
        auto read64 = [p](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
#if CPU(BIG_ENDIAN)
            uint64_t v = unalignedLoad<uint64_t>(p + off);
            return ((v >> 56) & 0xff) | ((v >> 40) & 0xff00) | ((v >> 24) & 0xff0000) | ((v >> 8) & 0xff000000)
                | ((v << 8) & 0xff00000000) | ((v << 24) & 0xff0000000000) | ((v << 40) & 0xff000000000000) | ((v << 56) & 0xff00000000000000);
#else
            return unalignedLoad<uint64_t>(p + off);
#endif
        };
        auto read32 = [p](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
#if CPU(BIG_ENDIAN)
            uint32_t v = unalignedLoad<uint32_t>(p + off);
            return static_cast<uint64_t>(((v >> 24) & 0xff) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | ((v << 24) & 0xff000000));
#else
            return static_cast<uint64_t>(unalignedLoad<uint32_t>(p + off));
#endif
        };
        auto readSmall = [p](size_t off, size_t k) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return (static_cast<uint64_t>(p[off]) << 56)
                | (static_cast<uint64_t>(p[off + (k >> 1)]) << 32)
                | static_cast<uint64_t>(p[off + k - 1]);
        };
        return rapidhashCore(len, read64, read32, readSmall);
    }

    // Runtime-only: hash Latin1-only UTF-16 data by compressing to 8-bit.
    ALWAYS_INLINE static uint64_t rapidhashCompressed16(const char16_t* p, size_t charCount)
    {
        // Each read at byte offset maps to char offset (1 output byte per char).
        auto read64 = [p](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            const char16_t* cp = p + off;
#if CPU(ARM64)
            uint16x8_t x;
            memcpy(&x, cp, sizeof(x));
            return vget_lane_u64(vreinterpret_u64_u8(vmovn_u16(x)), 0);
#elif CPU(X86_64)
            __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(cp));
            return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#else
            return static_cast<uint64_t>(static_cast<uint8_t>(cp[0]))
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[1])) << 8)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[2])) << 16)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[3])) << 24)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[4])) << 32)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[5])) << 40)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[6])) << 48)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[7])) << 56);
#endif
        };
        auto read32 = [p](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            const char16_t* cp = p + off;
#if CPU(ARM64)
            uint16x4_t x;
            memcpy(&x, cp, sizeof(x));
            uint16x8_t xWide = vcombine_u16(x, x);
            return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(xWide)), 0);
#elif CPU(X86_64)
            __m128i x = _mm_loadu_si64(reinterpret_cast<const __m128i*>(cp));
            return _mm_cvtsi128_si64(_mm_packus_epi16(x, x));
#else
            return static_cast<uint64_t>(static_cast<uint8_t>(cp[0]))
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[1])) << 8)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[2])) << 16)
                | (static_cast<uint64_t>(static_cast<uint8_t>(cp[3])) << 24);
#endif
        };
        auto readSmall = [p](size_t off, size_t k) ALWAYS_INLINE_LAMBDA -> uint64_t {
            return (static_cast<uint64_t>(static_cast<uint8_t>(p[off])) << 56)
                | (static_cast<uint64_t>(static_cast<uint8_t>(p[off + (k >> 1)])) << 32)
                | static_cast<uint64_t>(static_cast<uint8_t>(p[off + k - 1]));
        };
        return rapidhashCore(charCount, read64, read32, readSmall);
    }

    // Converter path: hash in 16-bit virtual space.
    // Each character (after conversion) produces 2 virtual bytes (LE).
    template<typename T, typename Converter>
    ALWAYS_INLINE static constexpr uint64_t rapidhashConverted(std::span<const T> data)
    {
        size_t len = data.size() * 2; // virtual byte count
        // Byte offset to character index: charIdx = byteOff / 2
        auto read64 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            size_t ci = off / 2;
            return static_cast<uint64_t>(Converter::convert(data[ci]))
                | (static_cast<uint64_t>(Converter::convert(data[ci + 1])) << 16)
                | (static_cast<uint64_t>(Converter::convert(data[ci + 2])) << 32)
                | (static_cast<uint64_t>(Converter::convert(data[ci + 3])) << 48);
        };
        auto read32 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            size_t ci = off / 2;
            return static_cast<uint64_t>(Converter::convert(data[ci]))
                | (static_cast<uint64_t>(Converter::convert(data[ci + 1])) << 16);
        };
        auto readSmall = [&](size_t off, size_t) ALWAYS_INLINE_LAMBDA -> uint64_t {
            // Only called for len=2 (1 char): virtual bytes = [lo, hi]
            char16_t c = Converter::convert(data[off / 2]);
            uint8_t lo = static_cast<uint8_t>(c);
            uint8_t hi = static_cast<uint8_t>(c >> 8);
            return (static_cast<uint64_t>(lo) << 56)
                | (static_cast<uint64_t>(hi) << 32)
                | static_cast<uint64_t>(hi);
        };
        return rapidhashCore(len, read64, read32, readSmall);
    }

    static bool isLatin1Only(const char16_t* chars, unsigned len)
    {
        for (unsigned i = 0; i < len; ++i) {
            if (chars[i] > 0xFF)
                return false;
        }
        return true;
    }

    template<typename T, typename Converter = DefaultConverter>
    static constexpr unsigned computeHashImpl(std::span<const T> characters)
    {
        uint64_t raw;

        if constexpr (!std::is_same_v<Converter, DefaultConverter>) {
            // Converter path (e.g., case-insensitive): hash in 16-bit virtual space.
            raw = rapidhashConverted<T, Converter>(characters);
        } else if constexpr (sizeof(T) == 1) {
            if (std::is_constant_evaluated()) {
                // Constexpr: index-based reads from typed span, no reinterpret_cast.
                raw = rapidhashBytes(characters);
            } else {
                // Runtime: unaligned loads for speed.
                raw = rapidhashRawBytes(reinterpret_cast<const uint8_t*>(characters.data()), characters.size());
            }
        } else {
            // 16-bit input.
            if (!std::is_constant_evaluated() && isLatin1Only(reinterpret_cast<const char16_t*>(characters.data()), characters.size())) {
                // Latin1-only UTF-16: compress to 8-bit for consistent hash with Latin1 strings.
                raw = rapidhashCompressed16(reinterpret_cast<const char16_t*>(characters.data()), characters.size());
            } else {
                // Non-Latin1 UTF-16 (or constexpr): hash raw bytes at double length.
                raw = rapidhashRawBytes16(characters);
            }
        }

        return static_cast<unsigned>(raw);
    }

    // Hash 16-bit data as raw bytes (constexpr-friendly).
    template<typename T>
    ALWAYS_INLINE static constexpr uint64_t rapidhashRawBytes16(std::span<const T> data)
    {
        static_assert(sizeof(T) == 2);
        size_t len = data.size() * 2;
        auto read64 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            size_t ci = off / 2;
            // Read 4 char16_t as 8 LE bytes.
            return static_cast<uint64_t>(data[ci] & 0xFF)
                | (static_cast<uint64_t>(data[ci] >> 8) << 8)
                | (static_cast<uint64_t>(data[ci + 1] & 0xFF) << 16)
                | (static_cast<uint64_t>(data[ci + 1] >> 8) << 24)
                | (static_cast<uint64_t>(data[ci + 2] & 0xFF) << 32)
                | (static_cast<uint64_t>(data[ci + 2] >> 8) << 40)
                | (static_cast<uint64_t>(data[ci + 3] & 0xFF) << 48)
                | (static_cast<uint64_t>(data[ci + 3] >> 8) << 56);
        };
        auto read32 = [&](size_t off) ALWAYS_INLINE_LAMBDA -> uint64_t {
            size_t ci = off / 2;
            return static_cast<uint64_t>(data[ci] & 0xFF)
                | (static_cast<uint64_t>(data[ci] >> 8) << 8)
                | (static_cast<uint64_t>(data[ci + 1] & 0xFF) << 16)
                | (static_cast<uint64_t>(data[ci + 1] >> 8) << 24);
        };
        auto readSmall = [&](size_t off, size_t k) ALWAYS_INLINE_LAMBDA -> uint64_t {
            // Extract raw bytes from char16_t data.
            auto getByte = [&](size_t byteIdx) -> uint64_t {
                size_t ci = (off + byteIdx) / 2;
                return ((off + byteIdx) & 1) ? static_cast<uint64_t>(data[ci] >> 8) : static_cast<uint64_t>(data[ci] & 0xFF);
            };
            return (getByte(0) << 56)
                | (getByte(k >> 1) << 32)
                | getByte(k - 1);
        };
        return rapidhashCore(len, read64, read32, readSmall);
    }
};

} // namespace WTF

using WTF::RapidHash;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
