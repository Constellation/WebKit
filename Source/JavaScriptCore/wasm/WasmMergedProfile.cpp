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
    uint32_t count = slot.count();
    m_count += count;

    if (CallProfile::isMegamorphic(boxedCallee)) {
        m_isMegamorphic = true;
        return;
    }

    // Keep in mind that count is updated concurrently.
    // Sum of all polymorphic counts may not be the same to the total count.
    if (auto* poly = CallProfile::polymorphic(boxedCallee)) {
        for (auto& profile : *poly) {
            if (auto* callee = CallProfile::monomorphic(profile.boxedCallee())) {
                auto addResult = m_callee.add(callee, profile.count());
                if (!addResult.isNewEntry)
                    addResult.iterator->value += profile.count();
            }
        }
    } else if (auto* callee = CallProfile::monomorphic(boxedCallee)) {
        auto addResult = m_callee.add(callee, count);
        if (!addResult.isNewEntry)
            addResult.iterator->value += count;
    }

    if (m_callee.size() > CallProfile::maxPolymorphicCallees) {
        m_callee.clear();
        m_isMegamorphic = true;
    }
}

} // namespace JSC::Wasm

#endif
