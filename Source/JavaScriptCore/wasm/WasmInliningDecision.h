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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmBaselineData.h>
#include <JavaScriptCore/WasmCallProfile.h>
#include <JavaScriptCore/WasmCallee.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace JSC::Wasm {

class MergedProfile;
class Module;
class InliningDecision;

class InliningNode {
    WTF_MAKE_TZONE_ALLOCATED(InliningNode);
    WTF_MAKE_NONMOVABLE(InliningNode);
public:
    using CallSite = Vector<InliningNode*, 3>;

    InliningNode(const IPIntCallee&, InliningNode* caller, uint32_t wasmSize, double relativeCallCount);

    void inlineNode(InliningDecision&);

    const IPIntCallee& callee() const { return m_callee; }
    const Vector<CallSite>& callSites() const { return m_callSites; }
    bool isInlined() const { return m_isInlined; }
    bool isUnused() const { return m_isUnused; }
    uint32_t depth() const { return m_depth; }
    double relativeCallCount() const { return m_relativeCallCount; }
    uint32_t wasmSize() const { return m_wasmSize; }
    double score() const;

private:
    const IPIntCallee& m_callee;
    Vector<CallSite> m_callSites;
    bool m_isInlined { false };
    bool m_isUnused { true };
    uint32_t m_depth { 0 };
    uint32_t m_wasmSize { 0 };
    double m_relativeCallCount { 0.0 };
};

class InliningDecision final {
    WTF_MAKE_TZONE_ALLOCATED(InliningDecision);
    WTF_MAKE_NONMOVABLE(InliningDecision);
    friend class Module;
    friend class InliningNode;
public:
    InliningDecision(Module& module, const IPIntCallee& rootCallee);

    MergedProfile* profileForCallee(const IPIntCallee&);

    void expand();

private:
    Module& m_module;
    SegmentedVector<InliningNode, 16> m_arena;
    UncheckedKeyHashMap<const IPIntCallee*, std::unique_ptr<MergedProfile>> m_profiles;
    InliningNode& m_root;
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
