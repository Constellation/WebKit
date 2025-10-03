/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSMicrotask.h"

#include "CatchScope.h"
#include "Debugger.h"
#include "DeferTermination.h"
#include "GlobalObjectMethodTable.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

JSValue runInternalMirotask(JSGlobalObject* globalObject, MicrotaskIdentifier, InternalMicrotask task, std::span<const JSValue> arguments)
{
    VM& vm = globalObject->vm();
    switch (task) {
    case InternalMicrotask::PromiseResolveThenableJobFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue promiseToResolve = arguments[1];

        if (!promise->inherits<JSInternalPromise>()) {
            if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
                return globalObject->promiseResolveThenableJobFastFallbackFunction();
        }

        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            auto* reaction = JSPromiseReaction::create(vm, globalObject->promiseReactionStructure(), promiseToResolve, jsUndefined(), jsUndefined(), jsUndefined(), promise->reactionsOrResult());
            promise->setReactionsOrResult(vm, reaction);
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled()) {
                if (globalObject->globalObjectMethodTable()->promiseRejectionTracker)
                    globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
                else
                    vm.promiseRejected(promise);
            }
            globalObject->queueMicrotask(globalObject->promiseReactionJobFunction(), promiseToResolve, jsUndefined(), promise->reactionsOrResult(), jsNumber(static_cast<int32_t>(JSPromise::Status::Rejected)));
            break;
        }
        case JSPromise::Status::Fulfilled: {
            globalObject->queueMicrotask(globalObject->promiseReactionJobFunction(), promiseToResolve, jsUndefined(), promise->reactionsOrResult(), jsNumber(static_cast<int32_t>(JSPromise::Status::Fulfilled)));
            break;
        }
        }
        promise->markAsHandled();
        return JSValue();
    }

    case InternalMicrotask::PromiseResolveThenableJobWithoutPromiseFast: {
        auto* promise = jsCast<JSPromise*>(arguments[0]);
        JSValue onFulfilled = arguments[1];
        JSValue onRejected = arguments[2];
        JSValue context = arguments[3];

        if (!promise->inherits<JSInternalPromise>()) {
            if (!promiseSpeciesWatchpointIsValid(vm, promise)) [[unlikely]]
                return globalObject->promiseResolveThenableJobWithoutPromiseFastFallbackFunction();
        }

        switch (promise->status()) {
        case JSPromise::Status::Pending: {
            auto* reaction = JSPromiseReaction::create(vm, globalObject->promiseReactionStructure(), jsUndefined(), onFulfilled, onRejected, context, promise->reactionsOrResult());
            promise->setReactionsOrResult(vm, reaction);
            break;
        }
        case JSPromise::Status::Rejected: {
            if (!promise->isHandled()) {
                if (globalObject->globalObjectMethodTable()->promiseRejectionTracker)
                    globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Handle);
                else
                    vm.promiseRejected(promise);
            }
            globalObject->queueMicrotask(globalObject->promiseReactionJobWithoutPromiseFunction(), onRejected, promise->reactionsOrResult(), context, jsUndefined());
            break;
        }
        case JSPromise::Status::Fulfilled: {
            globalObject->queueMicrotask(globalObject->promiseReactionJobWithoutPromiseFunction(), onFulfilled, promise->reactionsOrResult(), context, jsUndefined());
            break;
        }
        }

        promise->markAsHandled();
        return JSValue();
    }
    }

    return JSValue();
}

void runJSMicrotask(JSGlobalObject* globalObject, MicrotaskIdentifier identifier, JSValue job, std::span<const JSValue> arguments)
{
    if (job.isInt32AsAnyInt()) {
        job = runInternalMirotask(globalObject, identifier, static_cast<InternalMicrotask>(job.asInt32AsAnyInt()), arguments);
        if (!job)
            return;
    }

    if (!job.isObject()) [[unlikely]]
        return;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    auto handlerCallData = JSC::getCallData(job);
    if (!scope.clearExceptionExceptTermination()) [[unlikely]]
        return;
    ASSERT(handlerCallData.type != CallData::Type::None);

    unsigned count = 0;
    for (auto argument : arguments) {
        if (!argument)
            break;
        ++count;
    }

    if (globalObject->hasDebugger()) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->willRunMicrotask(globalObject, identifier);
        scope.clearException();
    }

    if (!vm.hasPendingTerminationException()) [[likely]] {
        profiledCall(globalObject, ProfilingReason::Microtask, job, handlerCallData, jsUndefined(), ArgList { std::bit_cast<EncodedJSValue*>(arguments.data()), count });
        scope.clearExceptionExceptTermination();
    }

    if (globalObject->hasDebugger()) [[unlikely]] {
        DeferTerminationForAWhile deferTerminationForAWhile(vm);
        globalObject->debugger()->didRunMicrotask(globalObject, identifier);
        scope.clearException();
    }
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
