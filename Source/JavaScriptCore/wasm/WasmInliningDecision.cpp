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


InliningNode::InliningNode(const IPIntCallee& callee, InliningNode* caller, size_t wasmSize, double relativeCallCount)
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

// candidate given the initial graph size and the already inlined wire bytes.
bool InliningDecision::canInline(InliningNode* target, size_t initialWasmSize, size_t inlinedWasmSize)
{
    size_t wasmSize = target->wasmSize();
    if (wasmSize > Options::maximumWasmCalleeSizeForInlining())
        return false;

    // For tiny functions, let's be a bit more generous.
    if (wasmSize < 12) {
        if (inlinedWasmSize > 100)
            inlinedWasmSize -= 100;
        else
            inlinedWasmSize = 0;
    }

    // For small-ish functions, the inlining budget is defined by the larger of
    // 1) the wasm_inlining_min_budget and
    // 2) the max_growth_factor * initial_wire_byte_size.
    // Inlining a little bit should always be fine even for tiny functions (1),
    // otherwise (2) makes sure that the budget scales in relation with the
    // original function size, to limit the compile time increase caused by
    // inlining.
    size_t budgetSmallFunction = std::max<size_t>(Options::wasmInliningMinimumBudget(), m_maxGrowthFactor * initialWasmSize);

    // For large functions, growing by the same factor would add too much
    // compilation effort, so we also apply a fixed cap. However, independent
    // of the budget cap, for large functions we should still allow a little
    // inlining, which is why we allow 10% of the graph size is the minimal
    // budget even for large functions that exceed the regular budget.
    //
    // Note for future tuning: it might make sense to allow 20% here, and in
    // turn perhaps lower --wasm-inlining-budget. The drawback is that this
    // would allow truly huge functions to grow even bigger; the benefit is
    // that we wouldn't fall off as steep a cliff when hitting the cap.
    size_t budgetLargeFunction = std::max<size_t>(m_budgetCap, initialWasmSize * 1.1);
    size_t totalSize = initialWasmSize + inlinedWasmSize + static_cast<size_t>(wasmSize);
    return totalSize < std::min<size_t>(budgetSmallFunction, budgetLargeFunction);
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
            size_t wasmSize = decision.m_module.moduleInformation().functionWasmSizeImportSpace(candidateCallee->index());
            auto& child = decision.m_arena.alloc(static_cast<const IPIntCallee&>(*candidateCallee), this, wasmSize, relativeCallCount);
            callSite.append(&child);
        }
    }
}

static double budgetScaleFactor(const Module& module)
{
    // If there are few small functions, that indicates that the toolchain
    // already performed significant inlining, so we reduce the budget
    // significantly as further inlining has diminishing benefits.
    // For both major knobs, we apply a smoothened step function based on
    // the module's percentage of small functions (sfp):
    //   sfp <= 25%: use "low" budget
    //   sfp >= 50%: use "high" budget
    //   25% < sfp < 50%: interpolate linearly between both budgets.
    double smallFunctionPercentage = module.moduleInformation().m_numSmallFunctions * 100.0 / module.moduleInformation().internalFunctionCount();
    if (smallFunctionPercentage <= 25)
        return 0;
    if (smallFunctionPercentage >= 50)
        return 1;

    return (smallFunctionPercentage - 25) / 25;
}

InliningDecision::InliningDecision(Module& module, const IPIntCallee& rootCallee)
    : m_module(module)
    , m_root(m_arena.alloc(rootCallee, nullptr, module.moduleInformation().functionWasmSizeImportSpace(rootCallee.index()), 1.0))
{
    double scaled = budgetScaleFactor(module);
    int highGrowth = Options::wasmInliningFactor();

    // A value of 1 would be equivalent to disabling inlining entirely.
    constexpr int lowestUsefulValue = 2;
    int lowGrowth = std::max(lowestUsefulValue, highGrowth - 3);
    m_maxGrowthFactor = lowGrowth * (1 - scaled) + highGrowth * scaled;

    double highCap = Options::wasmInliningBudget();
    double lowCap = highCap / 10;
    m_budgetCap = lowCap * (1 - scaled) + highCap * scaled;
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

    auto addChildrenToQueue = [&](InliningNode* target) {
        if (target->depth() >= Options::maximumWasmDepthForInlining())
            return;

        for (const auto& callSite : target->callSites()) {
            for (auto* node : callSite)
                queue.enqueue(node);
        }
    };

    uint32_t initialWasmSize = m_root.wasmSize();
    uint32_t inlinedWasmSize = 0;

    m_root.inlineNode(*this);
    ++m_inlinedCount;
    addChildrenToQueue(&m_root);

    while (!queue.isEmpty()) {
        if (m_inlinedCount >= Options::maximumWasmInliningCount())
            break;

        auto* target = queue.dequeue();

        if (target->wasmSize() >= 12) {
            if (target->score() < 0.0001)
                continue;
        }

        if (!canInline(target, initialWasmSize, inlinedWasmSize))
            continue;

        target->inlineNode(*this);
        ++m_inlinedCount;
        addChildrenToQueue(target);

        constexpr size_t oneLessCall = 6; // Guesstimated savings per call.
        size_t addition = target->wasmSize();
        if (addition >= oneLessCall)
            addition -= oneLessCall;
        inlinedWasmSize += addition;
    }
}

} // namespace JSC::Wasm

#endif
