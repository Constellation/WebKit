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
#include "WasmMergedProfile.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC::Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MergedProfile);

MergedProfile::MergedProfile(const IPIntCallee& callee)
    : m_callSites(callee.numCallProfiles())
{
}

void MergedProfile::CallSite::merge(const CallProfile& slot)
{
    EncodedJSValue boxedCallee = slot.boxedCallee();
    uint32_t speculativeTotalCount = slot.count();

    if (CallProfile::isMegamorphic(boxedCallee)) {
        m_count += speculativeTotalCount;
        m_isMegamorphic = true;
        return;
    }

    // Let's not use slot.count() as we are concurrently reading polymorphic callee.
    // Make m_count consistent with all the polymorphic callee's counts.
    unsigned addedCount = 0;
    if (auto* poly = CallProfile::polymorphic(boxedCallee)) {
        for (auto& profile : *poly) {
            if (auto* callee = CallProfile::monomorphic(profile.boxedCallee())) {
                auto addResult = m_callee.add(callee, profile.count());
                if (!addResult.isNewEntry)
                    addResult.iterator->value += profile.count();
                addedCount += profile.count();
            }
        }
    } else if (auto* callee = CallProfile::monomorphic(boxedCallee)) {
        auto addResult = m_callee.add(callee, speculativeTotalCount);
        if (!addResult.isNewEntry)
            addResult.iterator->value += speculativeTotalCount;
        addedCount += speculativeTotalCount;
    }
    if (m_callee.size() > CallProfile::maxPolymorphicCallees) {
        m_callee.clear();
        m_count += speculativeTotalCount;
        m_isMegamorphic = true;
    }

    m_count += addedCount;
}

auto MergedProfile::CallSite::candidates() const -> Candidates
{
    if (m_isMegamorphic)
        return { };
    if (m_callee.isEmpty())
        return { };

    if (m_callee.size() > CallProfile::maxPolymorphicCallees)
        return { };

    return Candidates { m_count, m_callee };
}

MergedProfile::Candidates::Candidates(uint32_t totalCount, const UncheckedKeyHashMap<Callee*, uint32_t>& callees)
    : m_totalCount(totalCount)
{
    uint32_t size = 0;
    for (auto [callee, count] : callees)
        m_callees[size++] = std::tuple { callee, count };
    m_size = size;
    auto mutableSpan = std::span { m_callees }.first(m_size);
    std::sort(mutableSpan.begin(), mutableSpan.end(),
        [&](const auto& lhs, const auto& rhs) {
            return std::get<1>(lhs) > std::get<1>(rhs);
        });
}


} // namespace JSC::Wasm

#endif
