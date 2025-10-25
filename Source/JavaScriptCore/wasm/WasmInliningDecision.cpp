/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2023 the V8 project authors. All rights reserved.
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
#include "WasmInliningDecision.h"

#include "WasmMergedProfile.h"
#include <JavaScriptCore/WasmModule.h>
#include <JavaScriptCore/WasmModuleInformation.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/PriorityQueue.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBASSEMBLY)

namespace JSC::Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InliningNode);
WTF_MAKE_TZONE_ALLOCATED_IMPL(InliningDecision);


InliningNode::InliningNode(const IPIntCallee& callee, InliningNode* caller, uint32_t wasmSize, double relativeCallCount)
    : m_callee(callee)
    , m_depth(caller ? caller->m_depth + 1 : 0)
    , m_wasmSize(wasmSize)
    , m_relativeCallCount(relativeCallCount)
{
}

double InliningNode::score() const
{
    if (!m_wasmSize)
        return 0.0;
    return m_relativeCallCount / m_wasmSize;
}

void InliningNode::inlineNode(InliningDecision& decision)
{
    m_isInlined = true;
    auto* profile = decision.profileForCallee(m_callee);
    if (!profile->merged())
        return;

    m_isUnused = false;
    m_callSites.grow(profile->size());

    for (unsigned index = 0; index < m_callSites.size(); ++index) {
        if (!profile->isCalled(index))
            continue;

        if (profile->isMegamorphic(index))
            continue;

        auto& callSite = m_callSites[index];
        auto candidates = profile->candidates(index);
        for (auto& [candidateCallee, callCount] : candidates.callees()) {
            if (candidateCallee->compilationMode() != Wasm::CompilationMode::IPIntMode)
                continue;

            double relativeCallCount = 0;
            if (candidates.totalCount())
                relativeCallCount = callCount / static_cast<double>(candidates.totalCount());
            uint32_t wasmSize = decision.m_module.moduleInformation().functionWasmSizeImportSpace(candidateCallee->index());
            auto& child = decision.m_arena.alloc(static_cast<const IPIntCallee&>(*candidateCallee), this, wasmSize, relativeCallCount);
            callSite.append(&child);
        }
    }
}

InliningDecision::InliningDecision(Module& module, const IPIntCallee& rootCallee)
    : m_module(module)
    , m_root(m_arena.alloc(rootCallee, nullptr, module.moduleInformation().functionWasmSizeImportSpace(rootCallee.index()), 1.0))
{
}

MergedProfile* InliningDecision::profileForCallee(const IPIntCallee& callee)
{
    return m_profiles.ensure(&callee, [&]{
        return m_module.createMergedProfile(callee);
    }).iterator->value.get();
}

static bool isHigherPriority(InliningNode* const& lhs, InliningNode* const& rhs)
{
    return std::tuple { lhs->score(), lhs->callee().index(), lhs } < std::tuple { rhs->score(), rhs->callee().index(), rhs };
}

void InliningDecision::expand()
{
    PriorityQueue<InliningNode*, isHigherPriority> queue;

    m_root.inlineNode(*this);
    for (const auto& callSite : m_root.callSites()) {
        for (auto* node : callSite)
            queue.enqueue(node);
    }

    queue.isEmpty();
}

} // namespace JSC::Wasm

#endif
