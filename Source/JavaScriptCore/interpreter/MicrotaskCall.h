/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/CallLinkInfoBase.h>
#include <JavaScriptCore/ExceptionHelpers.h>
#include <JavaScriptCore/Interpreter.h>
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/JSFunctionInlines.h>
#include <JavaScriptCore/ProtoCallFrameInlines.h>
#include <JavaScriptCore/VMInlines.h>
#include <array>
#include <wtf/ForbidHeapAllocation.h>

namespace JSC {

// MicrotaskCall caches a call site keyed on FunctionExecutable*.
// Unlike CachedCall (which is keyed on a specific JSFunction), MicrotaskCall
// allows the callee JSFunction to differ across calls as long as they share the
// same FunctionExecutable (i.e. different closure instances of the same function).
// This lets MicrotaskQueue::drainImpl skip the prepareForExecution compilation
// check on repeated microtask invocations with different closure objects.
//
// MicrotaskCall is only used inside an active VM entry (MicrotaskQueue::drainImpl),
// so it does not create its own VMEntryScope.
class MicrotaskCall : public CallLinkInfoBase {
    WTF_MAKE_NONCOPYABLE(MicrotaskCall);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    // Matches the static_assert(sizeof...(args) <= 6) in callMicrotask.
    static constexpr unsigned maxCallArguments = 6;

    explicit MicrotaskCall(VM& vm)
        : CallLinkInfoBase(CallSiteType::MicrotaskCall)
        , m_vm(vm)
    { }

    ~MicrotaskCall()
    {
        m_addressForCall = nullptr;
    }

    // (Re-)initialize the cache for `function`.
    // Returns false (and throws) on error.
    bool initialize(JSGlobalObject*, JSFunction*);

    // Fast call: same executable, potentially a different JSFunction instance.
    template<typename... Args> requires (std::is_convertible_v<Args, JSValue> && ...)
    JSValue callWithArguments(JSGlobalObject*, JSFunction*, JSValue thisValue, JSCell* context, Args...);

    bool isInitializedFor(FunctionExecutable* executable) const
    {
#if ASSERT_ENABLED
        if (!m_initialized)
            return false;
#endif
        return m_functionExecutable == executable;
    }

    FunctionExecutable* functionExecutable() { return m_functionExecutable; }

    void unlinkOrUpgradeImpl(VM&, CodeBlock* oldCodeBlock, CodeBlock* newCodeBlock);
    void relink(JSFunction*);

private:
    VM& m_vm;
    ProtoCallFrame m_protoCallFrame;

    FunctionExecutable* m_functionExecutable { nullptr };
    void* m_addressForCall { nullptr };
    unsigned m_numParameters { 0 };
#if ASSERT_ENABLED
    bool m_initialized { false };
#endif

    // Argument buffer: placed last since it is only written on the slow call path.
    std::array<EncodedJSValue, maxCallArguments> m_arguments { };

    friend class Interpreter;
};

} // namespace JSC
