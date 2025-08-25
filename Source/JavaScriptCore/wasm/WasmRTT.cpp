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
#include "WasmRTT.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/bmalloc.h>
#include <bmalloc/bmalloc_heap.h>
#include <bmalloc/bmalloc_heap_config.h>
#include <bmalloc/bmalloc_heap_inlines.h>
#include <bmalloc/bmalloc_heap_ref.h>
#include <bmalloc/pas_page_sharing_pool.h>
#include <bmalloc/pas_primitive_heap_ref.h>
#include <bmalloc/pas_probabilistic_guard_malloc_allocator.h>
#include <bmalloc/pas_scavenger.h>
#include <bmalloc/pas_thread_local_cache.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::Wasm {

#if CPU(ADDRESS64) && !ENABLE(WASM_RTT_ID_WITH_SHIFT)

template<size_t minimumSize, size_t minimumAlignment, typename Tag>
class RegionHeap {
public:
    static constexpr bmalloc_type heapType { BMALLOC_TYPE_INITIALIZER(minimumSize, minimumAlignment, "RegionHeap") };

    static pas_primitive_heap_ref* heap()
    {
        static pas_primitive_heap_ref staticHeap { BMALLOC_AUXILIARY_HEAP_REF_INITIALIZER(&heapType, pas_bmalloc_heap_ref_kind_compact) };
        return &staticHeap;
    }

    RegionHeap(std::span<uint8_t> reserved)
        : m_reserved(reserved)
    {
        // Do not include the first minimumSize * 2 region to make 0 and 1 as a special meaning.
        bmalloc_force_auxiliary_heap_into_reserved_memory(heap(), std::bit_cast<uintptr_t>(reserved.data() + minimumSize * 2),  std::bit_cast<uintptr_t>(reserved.data() + reserved.size()));
    }

    void* tryAllocate(size_t size, size_t alignment)
    {
        return bmalloc_try_allocate_auxiliary_with_alignment_inline(heap(), size, alignment, pas_always_compact_allocation_mode);
    }

    void free(void* pointer)
    {
        bmalloc_deallocate_inline(pointer);
    }

    uintptr_t startAddress() { return std::bit_cast<uintptr_t>(m_reserved.data()); }
    size_t size() { return m_reserved.size(); }

private:
    std::span<uint8_t> m_reserved;
};

using RTTHeap = RegionHeap<sizeof(RTT), alignof(RTT), RTT>;
static RTTHeap& rttHeap()
{
    static LazyNeverDestroyed<RTTHeap> heap;
    static std::once_flag onceKey;
    std::call_once(onceKey,
        [&] {
            auto memory = reinterpret_cast<uint8_t*>(OSAllocator::tryReserveUncommittedAligned(rttHeapAddressSize, rttHeapAddressSize, OSAllocator::FastMallocPages));
            RELEASE_ASSERT(memory);
            heap.construct(std::span { memory, rttHeapAddressSize });
        });
    return heap.get();
}

uintptr_t RTTID::startOfRTTHeap()
{
    return rttHeap().startAddress();
}

size_t RTTID::sizeOfRTTHeap()
{
    return rttHeap().size();
}

#endif

RTT::RTT(RTTKind kind)
    : TrailingArrayType(1)
    , m_kind(kind)
    , m_displaySizeExcludingThis(size() - 1)
{
    at(0) = this;
}

RTT::RTT(RTTKind kind, const RTT& supertype)
    : TrailingArrayType(supertype.size() + 1)
    , m_kind(kind)
    , m_displaySizeExcludingThis(size() - 1)
{
    ASSERT(supertype.size() == (supertype.displaySizeExcludingThis() + 1));
    memcpySpan(span(), supertype.span());
    at(supertype.size()) = this;
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind)
{
    auto* memory = rttHeap().tryAllocate(allocationSize(/* itself */ 1), alignof(RTT));
    if (!memory) [[unlikely]]
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind));
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind, const RTT& supertype)
{
    auto* memory = rttHeap().tryAllocate(allocationSize(supertype.size() + 1), alignof(RTT));
    if (!memory) [[unlikely]]
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind, supertype));
}

void RTT::operator delete(RTT* rtt, std::destroying_delete_t)
{
    rtt->~RTT();
    rttHeap().free(rtt);
}

bool RTT::isSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() < parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

bool RTT::isStrictSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() <= parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
