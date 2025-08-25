/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JSCConfig.h>
#include <JavaScriptCore/WasmOps.h>
#include <compare>
#include <wtf/FixedVector.h>
#include <wtf/HashTraits.h>
#include <wtf/StdIntExtras.h>
#include <wtf/ThreadSafeRefCounted.h>

#if HAVE(36BIT_ADDRESS)
#define RTT_ALIGNMENT alignas(16)
#else
#define RTT_ALIGNMENT
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::Wasm {

// An RTT encodes subtyping information in a way that is suitable for executing
// runtime subtyping checks, e.g., for ref.cast and related operations. RTTs are also
// used to facilitate static subtyping checks for references.
//
// It contains a display data rtt that allows subtyping of references to be checked in constant time.
//
// See https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#runtime-types for an explanation of displays.
enum class RTTKind : uint8_t {
    Function,
    Array,
    Struct
};

using DisplayCount = uint32_t;

class RTT;

#if CPU(ADDRESS64)

// We would like to define this value in PlatformEnable.h, but it is not possible since the following is relying on MACH_VM_MAX_ADDRESS.
#if CPU(ARM64) && OS(DARWIN) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#if MACH_VM_MAX_ADDRESS_RAW < (1ULL << 36)
#define ENABLE_WASM_RTT_ID_WITH_SHIFT 1
static_assert(MACH_VM_MAX_ADDRESS_RAW == MACH_VM_MAX_ADDRESS);
#endif
#endif

#if !ENABLE(WASM_RTT_ID_WITH_SHIFT)
#if defined(WASM_RTT_HEAP_ADDRESS_SIZE_IN_MB) && WASM_RTT_HEAP_ADDRESS_SIZE_IN_MB > 0
constexpr uintptr_t rttHeapAddressSize = WASM_RTT_HEAP_ADDRESS_SIZE_IN_MB * MB;
#elif PLATFORM(PLAYSTATION)
constexpr uintptr_t rttHeapAddressSize = 128 * MB;
#elif PLATFORM(IOS_FAMILY) && CPU(ARM64) && !CPU(ARM64E)
constexpr uintptr_t rttHeapAddressSize = 512 * MB;
#else
constexpr uintptr_t rttHeapAddressSize = 4 * GB;
#endif
#endif // !ENABLE(WASM_RTT_ID_WITH_SHIFT)

#endif // CPU(ADDRESS64)

class RTTID {
public:
#if ENABLE(WASM_RTT_ID_WITH_SHIFT)
    // ENABLE(WASM_RTT_ID_WITH_SHIFT) is used when our virtual memory space is limited (specifically, less than or equal to 36 bit) while pointer is 64 bit.
    // In that case, we round up RTTs size with 32 bytes instead of 16 bytes. This ensures that lower 5 bit become zero for RTT.
    // By shifting this address with 4, we can encode 36 bit address into 32 bit RTTID. And we can ensure that RTTID's lowest bit is still zero
    // because we round RTT size with 32 bytes. This lowest bit is used for nuke bit.
    static constexpr unsigned encodeShiftAmount = 4;
#elif CPU(ADDRESS64)
    static constexpr CPURegister rttIDMask = rttHeapAddressSize - 1;
#endif

    constexpr RTTID() = default;
    constexpr RTTID(RTTID const&) = default;
    constexpr RTTID& operator=(RTTID const&) = default;

    inline RTT* decode() const;
    inline RTT* tryDecode() const;
    static RTTID encode(const RTT*);

    explicit operator bool() const { return !!m_bits; }
    friend auto operator<=>(const RTTID&, const RTTID&) = default;
    constexpr uint32_t bits() const { return m_bits; }

    constexpr RTTID(WTF::HashTableDeletedValueType) : m_bits(1) { }
    bool isHashTableDeletedValue() const { return *this == RTTID(WTF::HashTableDeletedValue); }

    static uintptr_t startOfRTTHeap();
    static size_t sizeOfRTTHeap();

private:
    explicit constexpr RTTID(uint32_t bits) : m_bits(bits) { }

    uint32_t m_bits { 0 };
};
static_assert(sizeof(RTTID) == sizeof(uint32_t));

#if ENABLE(WASM_RTT_ID_WITH_SHIFT)

ALWAYS_INLINE RTT* RTTID::decode() const
{
    return std::bit_cast<RTT*>(static_cast<uintptr_t>(m_bits) << encodeShiftAmount);
}

ALWAYS_INLINE RTT* RTTID::tryDecode() const
{
    // Take care to only use the bits from m_bits in the rtt's address reservation.
    uintptr_t address = static_cast<uintptr_t>(m_bits) << encodeShiftAmount;
    return std::bit_cast<RTT*>(address);
}

ALWAYS_INLINE RTTID RTTID::encode(const RTT* rtt)
{
    ASSERT(rtt);
    auto result = RTTID(std::bit_cast<uintptr_t>(rtt) >> encodeShiftAmount);
    ASSERT(result.decode() == rtt);
    return result;
}

#elif CPU(ADDRESS64)

ALWAYS_INLINE RTT* RTTID::decode() const
{
    // Take care to only use the bits from m_bits in the rtt's address reservation.
    return std::bit_cast<RTT*>((static_cast<uintptr_t>(m_bits) & rttIDMask) + startOfRTTHeap());
}

ALWAYS_INLINE RTT* RTTID::tryDecode() const
{
    // Take care to only use the bits from m_bits in the rtt's address reservation.
    uintptr_t offset = static_cast<uintptr_t>(m_bits);
    if (offset >= sizeOfRTTHeap())
        return nullptr;
    return std::bit_cast<RTT*>((offset & rttIDMask) + startOfRTTHeap());
}

ALWAYS_INLINE RTTID RTTID::encode(const RTT* rtt)
{
    ASSERT(rtt);
    ASSERT(startOfRTTHeap() <= std::bit_cast<uintptr_t>(rtt) && std::bit_cast<uintptr_t>(rtt) < startOfRTTHeap() + rttHeapAddressSize);
    auto result = RTTID(std::bit_cast<uintptr_t>(rtt) & rttIDMask);
    ASSERT(result.decode() == rtt);
    return result;
}

#else // CPU(ADDRESS64)

ALWAYS_INLINE RTT* RTTID::decode() const
{
    return std::bit_cast<RTT*>(m_bits);
}

ALWAYS_INLINE RTT* RTTID::tryDecode() const
{
    return std::bit_cast<RTT*>(m_bits);
}

ALWAYS_INLINE RTTID RTTID::encode(const RTT* rtt)
{
    ASSERT(rtt);
    return RTTID(std::bit_cast<uint32_t>(rtt));
}

#endif

struct RTTIDHash {
    static unsigned hash(const RTTID& key) { return key.bits(); }
    static bool equal(const RTTID& a, const RTTID& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};


class RTT_ALIGNMENT RTT final : public ThreadSafeRefCounted<RTT>, private TrailingArray<RTT, const RTT*> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(RTT);
    WTF_MAKE_NONMOVABLE(RTT);
    using TrailingArrayType = TrailingArray<RTT, const RTT*>;
    friend TrailingArrayType;
public:
    RTT() = delete;

    static RefPtr<RTT> tryCreate(RTTKind);
    static RefPtr<RTT> tryCreate(RTTKind, const RTT&);

    void operator delete(RTT*, std::destroying_delete_t);

    RTTKind kind() const { return m_kind; }
    DisplayCount displaySizeExcludingThis() const { return m_displaySizeExcludingThis; }
    const RTT* displayEntry(DisplayCount i) const { return at(i); }

    bool isSubRTT(const RTT& other) const;
    bool isStrictSubRTT(const RTT& other) const;

    static constexpr ptrdiff_t offsetOfKind() { return OBJECT_OFFSETOF(RTT, m_kind); }
    static constexpr ptrdiff_t offsetOfDisplaySizeExcludingThis() { return OBJECT_OFFSETOF(RTT, m_displaySizeExcludingThis); }
    using TrailingArrayType::offsetOfData;

private:
    explicit RTT(RTTKind kind);
    RTT(RTTKind, const RTT& supertype);

    const RTTKind m_kind;
    unsigned m_displaySizeExcludingThis { };
};

} // namespace JSC::Wasm
namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Wasm::RTTID> : JSC::Wasm::RTTIDHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Wasm::RTTID> : SimpleClassHashTraits<JSC::Wasm::RTTID> {
    static constexpr bool emptyValueIsZero = true;
};

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
