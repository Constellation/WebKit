/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "B3Operations.h"

#include <bit>
#include <wtf/Int128.h>
#include <wtf/UnalignedAccess.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(B3_JIT)

namespace JSC::B3 {

namespace {

using WTF::WidestUnalignedUnit;

constexpr size_t maxFastFillCount = 4 * sizeof(WidestUnalignedUnit);

// Writes count bytes as two overlapping runs of unitsPerEnd stores, so it needs
// unitsPerEnd * sizeof(T) <= count <= 2 * unitsPerEnd * sizeof(T). Every store writes the same
// splatted value, so unlike the copy case the order of the stores does not matter.
template<typename T, unsigned unitsPerEnd>
ALWAYS_INLINE void fillOverlappingEnds(uint8_t* dst, T value, size_t count)
{
    for (unsigned i = 0; i < unitsPerEnd; ++i)
        WTF::unalignedStore<T>(dst + i * sizeof(T), value);
    for (unsigned i = 0; i < unitsPerEnd; ++i)
        WTF::unalignedStore<T>(dst + count - (unitsPerEnd - i) * sizeof(T), value);
}

} // namespace

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryCopy, void, (void* dst, const void* src, size_t count))
{
    // Short copies dominate this operation, because memory.copy is what a compiler targeting wasm
    // emits for memcpy. libc's memmove spends more on dispatching by size than a short copy costs,
    // so handle the short counts here. One unsigned compare rejects both the too-short and the
    // too-long counts.
    if (count - sizeof(uint32_t) <= WTF::maxSmallCopySize - sizeof(uint32_t)) {
        WTF::copySmallMemory<sizeof(uint32_t)>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), count);
        return;
    }
    memmove(dst, src, count);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryFill, void, (void* dst, int32_t target, size_t count))
{
    if (count - 1 <= maxFastFillCount - 1) {
        auto* to = static_cast<uint8_t*>(dst);
        uint64_t splat = static_cast<uint8_t>(target) * 0x0101010101010101ULL;
        WidestUnalignedUnit widest = splat;
        if constexpr (sizeof(WidestUnalignedUnit) > sizeof(uint64_t))
            widest = (static_cast<WidestUnalignedUnit>(splat) << 64) | splat;
        if (count < sizeof(uint16_t))
            fillOverlappingEnds<uint8_t, 1>(to, static_cast<uint8_t>(splat), count);
        else if (count < sizeof(uint32_t))
            fillOverlappingEnds<uint16_t, 1>(to, static_cast<uint16_t>(splat), count);
        else if (count < sizeof(uint64_t))
            fillOverlappingEnds<uint32_t, 1>(to, static_cast<uint32_t>(splat), count);
        else if (count < 2 * sizeof(uint64_t))
            fillOverlappingEnds<uint64_t, 1>(to, splat, count);
        else if (count < 2 * sizeof(WidestUnalignedUnit))
            fillOverlappingEnds<WidestUnalignedUnit, 1>(to, widest, count);
        else
            fillOverlappingEnds<WidestUnalignedUnit, 2>(to, widest, count);
        return;
    }
    memset(dst, target, count);
}

#if CPU(LITTLE_ENDIAN)
// The index of the first mismatching byte within a unit of 2^shift XORed bytes, or the unit size
// when the unit matches. The caller guarantees count >= sizeof(T).
template<typename T>
ALWAYS_INLINE uint64_t compareUnit(const uint8_t* a, const uint8_t* b)
{
    T difference = WTF::unalignedLoad<T>(a) ^ WTF::unalignedLoad<T>(b);
    if (!difference)
        return sizeof(T);
    return std::countr_zero(difference) / 8;
}
#endif

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryCompare, uint64_t, (const void* a, const void* b, size_t count))
{
    // The number of leading bytes at which the two ranges agree. Short compares dominate, so
    // counts up to four words are done with straight-line overlapping-ends loads: the head units
    // are compared in order, then the tail units, and bytes checked by both agree by the time a
    // tail unit is looked at, so its first mismatch is always in its not-yet-checked part. Keeping
    // the hot sizes out of a loop also avoids mispredicting the loop-exit branch when the count
    // varies from call to call.
    auto* aBytes = static_cast<const uint8_t*>(a);
    auto* bBytes = static_cast<const uint8_t*>(b);
#if CPU(LITTLE_ENDIAN)
    if (count >= sizeof(uint64_t)) {
        uint64_t matched = compareUnit<uint64_t>(aBytes, bBytes);
        if (matched < sizeof(uint64_t))
            return matched;
        if (count > 2 * sizeof(uint64_t)) {
            if (count <= 4 * sizeof(uint64_t)) {
                matched = compareUnit<uint64_t>(aBytes + sizeof(uint64_t), bBytes + sizeof(uint64_t));
                if (matched < sizeof(uint64_t))
                    return sizeof(uint64_t) + matched;
                matched = compareUnit<uint64_t>(aBytes + count - 2 * sizeof(uint64_t), bBytes + count - 2 * sizeof(uint64_t));
                if (matched < sizeof(uint64_t))
                    return (count - 2 * sizeof(uint64_t)) + matched;
                return (count - sizeof(uint64_t)) + compareUnit<uint64_t>(aBytes + count - sizeof(uint64_t), bBytes + count - sizeof(uint64_t));
            }
            // Branching out of the loop on a mismatch keeps the position arithmetic off the hot
            // path; the overlapped final word runs once, so it can stay branchless.
            uint64_t index = sizeof(uint64_t);
            while (index + sizeof(uint64_t) <= count) {
                uint64_t difference = WTF::unalignedLoad<uint64_t>(aBytes + index) ^ WTF::unalignedLoad<uint64_t>(bBytes + index);
                if (difference)
                    return index + std::countr_zero(difference) / 8;
                index += sizeof(uint64_t);
            }
            if (index < count)
                return (count - sizeof(uint64_t)) + compareUnit<uint64_t>(aBytes + count - sizeof(uint64_t), bBytes + count - sizeof(uint64_t));
            return count;
        }
        if (count > sizeof(uint64_t))
            return (count - sizeof(uint64_t)) + compareUnit<uint64_t>(aBytes + count - sizeof(uint64_t), bBytes + count - sizeof(uint64_t));
        return count;
    }
    if (count >= sizeof(uint32_t)) {
        uint64_t matched = compareUnit<uint32_t>(aBytes, bBytes);
        if (matched < sizeof(uint32_t))
            return matched;
        if (count > sizeof(uint32_t))
            return (count - sizeof(uint32_t)) + compareUnit<uint32_t>(aBytes + count - sizeof(uint32_t), bBytes + count - sizeof(uint32_t));
        return count;
    }
    if (count >= sizeof(uint16_t)) {
        uint64_t matched = compareUnit<uint16_t>(aBytes, bBytes);
        if (matched < sizeof(uint16_t))
            return matched;
        if (count > sizeof(uint16_t))
            return (count - sizeof(uint16_t)) + compareUnit<uint16_t>(aBytes + count - sizeof(uint16_t), bBytes + count - sizeof(uint16_t));
        return count;
    }
    if (count)
        return aBytes[0] == bBytes[0] ? 1 : 0;
    return 0;
#else
    for (uint64_t index = 0; index < count; ++index) {
        if (aBytes[index] != bBytes[index])
            return index;
    }
    return count;
#endif
}

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
