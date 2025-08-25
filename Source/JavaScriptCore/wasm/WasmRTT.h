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

#include <JavaScriptCore/WasmOps.h>
#include <wtf/FixedVector.h>
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
// It contains a display data structure that allows subtyping of references to be checked in constant time.
//
// See https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#runtime-types for an explanation of displays.
enum class RTTKind : uint8_t {
    Function,
    Array,
    Struct
};

using DisplayCount = uint32_t;

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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
