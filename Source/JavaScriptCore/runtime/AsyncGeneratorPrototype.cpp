/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "AsyncGeneratorPrototype.h"

#include "BuiltinNames.h"
#include "IteratorOperations.h"
#include "JSAsyncGenerator.h"
#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSPromise.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "ObjectConstructor.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(asyncGeneratorProtoFuncNext);
static JSC_DECLARE_HOST_FUNCTION(asyncGeneratorProtoFuncReturn);
static JSC_DECLARE_HOST_FUNCTION(asyncGeneratorProtoFuncThrow);

const ClassInfo AsyncGeneratorPrototype::s_info = { "AsyncGenerator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(AsyncGeneratorPrototype) };

// Check if queue is empty
static inline bool asyncGeneratorQueueIsEmpty(JSAsyncGenerator* generator)
{
    return generator->queueFirst().isNull();
}

// Enqueue a request item to the generator's queue
static void asyncGeneratorQueueEnqueue(VM& vm, JSAsyncGenerator* generator, JSPromiseReaction* item)
{
    ASSERT(!item->next());

    JSValue queueFirst = generator->queueFirst();

    if (queueFirst.isNull()) {
        ASSERT(generator->queueLast().isNull());

        generator->setQueueFirst(vm, item);
        generator->setQueueLast(vm, item);
    } else {
        auto* last = jsCast<JSPromiseReaction*>(generator->queueLast());
        last->setNext(vm, item);
        generator->setQueueLast(vm, item);
    }
}

// Dequeue a request item from the generator's queue
static JSPromiseReaction* asyncGeneratorQueueDequeue(VM& vm, JSAsyncGenerator* generator)
{
    ASSERT(!asyncGeneratorQueueIsEmpty(generator));

    auto* result = jsCast<JSPromiseReaction*>(generator->queueFirst());
    auto* updatedFirst = result->next();

    generator->setQueueFirst(vm, updatedFirst ? JSValue(updatedFirst) : jsNull());

    if (!updatedFirst)
        generator->setQueueLast(vm, jsNull());

    return result;
}

// Check if generator is in execution state
static bool isExecutionState(JSAsyncGenerator* generator)
{
    int32_t state = generator->state();
    int32_t reason = generator->suspendReason();

    return (state > 0 && reason == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::None))
        || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing)
        || reason == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await);
}

// Check if generator is in suspend-yield state
static bool isSuspendYieldState(JSAsyncGenerator* generator)
{
    int32_t state = generator->state();
    int32_t reason = generator->suspendReason();

    return (state > 0 && reason == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield))
        || state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::SuspendedYield);
}

// Forward declarations for async operations
static void asyncGeneratorYield(JSGlobalObject*, JSAsyncGenerator*, JSValue value, int32_t resumeMode);

// Reject the current generator request
void asyncGeneratorReject(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue exception)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSAsyncGenerator*>(generator));

    auto* item = asyncGeneratorQueueDequeue(vm, generator);
    JSPromise* promise = jsCast<JSPromise*>(item->promise());
    ASSERT(promise);

    promise->reject(vm, globalObject, exception);
    RETURN_IF_EXCEPTION(scope, void());

    asyncGeneratorResumeNext(globalObject, generator);
}

// Resolve the current generator request
void asyncGeneratorResolve(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue value, bool done)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(jsDynamicCast<JSAsyncGenerator*>(generator));

    auto* item = asyncGeneratorQueueDequeue(vm, generator);
    JSPromise* promise = jsCast<JSPromise*>(item->promise());
    ASSERT(promise);

    // Create iterator result object { value, done }
    auto* iteratorResult = createIteratorResultObject(globalObject, value, done);

    promise->resolve(globalObject, iteratorResult);
    RETURN_IF_EXCEPTION(scope, void());

    asyncGeneratorResumeNext(globalObject, generator);
}

// Yield operation
static void asyncGeneratorYield(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue value, int32_t)
{
    VM& vm = globalObject->vm();

    generator->setSuspendReason(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await));

    JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, value, InternalMicrotask::AsyncGeneratorYieldAwaited, generator);
}

void doAsyncGeneratorBodyCall(JSGlobalObject* globalObject, JSAsyncGenerator* generator, JSValue resumeValue, int32_t resumeMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode) && isSuspendYieldState(generator)) {
        generator->setSuspendReason(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await));

        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, resumeValue, InternalMicrotask::AsyncGeneratorBodyCallReturn, generator);
        return;
    }

    JSValue value = jsUndefined();
    int32_t state = generator->state();

    generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing));
    generator->setSuspendReason(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::None));

    JSValue generatorFunction = generator->next();
    JSValue generatorThis = generator->thisValue();
    JSValue generatorFrame = generator->frame();

    std::array<EncodedJSValue, 5> args = { {
        JSValue::encode(generator),
        JSValue::encode(jsNumber(state)),
        JSValue::encode(resumeValue),
        JSValue::encode(jsNumber(resumeMode)),
        JSValue::encode(generatorFrame),
    } };

    value = callMicrotask(globalObject, generatorFunction, generatorThis, generator, ArgList { args.data(), args.size() }, "handler is not a function"_s);
    if (scope.exception()) [[unlikely]] {
        JSValue error = scope.exception()->value();
        scope.clearException();

        generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        generator->setSuspendReason(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::None));

        asyncGeneratorReject(globalObject, generator, error);
        return;
    }

    state = generator->state();
    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing)) {
        generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        state = static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed);
    }

    int32_t reason = generator->suspendReason();
    if (reason == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Await)) {
        JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, value, InternalMicrotask::AsyncGeneratorBodyCallNormal, generator);
        return;
    }

    if (reason == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::Yield)) {
        asyncGeneratorYield(globalObject, generator, value, resumeMode);
        return;
    }

    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        ASSERT(generator->state() == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
        generator->setSuspendReason(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorSuspendReason::None));
        asyncGeneratorResolve(globalObject, generator, value, true);
    }
}

// Callbacks for resume next
JSC_DEFINE_HOST_FUNCTION(asyncGeneratorResumeNextOnRejected, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue error = callFrame->uncheckedArgument(0);
    JSAsyncGenerator* generator = jsCast<JSAsyncGenerator*>(callFrame->uncheckedArgument(1));

    VM& vm = globalObject->vm();
    generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));

    asyncGeneratorReject(globalObject, generator, error);
    return encodedJSUndefined();
}

// Resume next operation in the queue (already non-static)
void asyncGeneratorResumeNext(JSGlobalObject* globalObject, JSAsyncGenerator* generator)
{
    VM& vm = globalObject->vm();

    ASSERT(jsDynamicCast<JSAsyncGenerator*>(generator));

    int32_t state = generator->state();

    ASSERT(state != static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Executing));

    if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::AwaitingReturn))
        return;

    if (asyncGeneratorQueueIsEmpty(generator))
        return;

    auto* next = jsCast<JSPromiseReaction*>(generator->queueFirst());
    JSValue nextValue = next->onFulfilled();
    int32_t resumeMode = next->onRejected().asInt32AsAnyInt();

    if (resumeMode != static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)) {
        if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::SuspendedStart)) {
            generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed));
            state = static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed);
        }

        if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
            if (resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode)) {
                generator->setState(vm, static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::AwaitingReturn));

                JSPromise::resolveWithInternalMicrotaskForAsyncAwait(globalObject, nextValue, InternalMicrotask::AsyncGeneratorResumeNext, generator);
                return;
            }

            ASSERT(resumeMode == static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode));
            asyncGeneratorReject(globalObject, generator, nextValue);
            return;
        }
    } else if (state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::Completed)) {
        asyncGeneratorResolve(globalObject, generator, jsUndefined(), true);
        return;
    }

    ASSERT(state == static_cast<int32_t>(JSAsyncGenerator::AsyncGeneratorState::SuspendedStart) || isSuspendYieldState(generator));
    doAsyncGeneratorBodyCall(globalObject, generator, nextValue, resumeMode);
}

// Enqueue a request to the async generator
static JSValue asyncGeneratorEnqueue(JSGlobalObject* globalObject, JSValue generatorValue, JSValue value, int32_t resumeMode)
{
    VM& vm = globalObject->vm();

    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

    JSAsyncGenerator* generator = jsDynamicCast<JSAsyncGenerator*>(generatorValue);
    if (!generator) {
        promise->reject(vm, globalObject, createTypeError(globalObject, "|this| should be an async generator"_s));
        return promise;
    }

    auto* queueItem = JSPromiseReaction::create(vm, promise, value, jsNumber(resumeMode), jsNull(), nullptr);
    asyncGeneratorQueueEnqueue(vm, generator, queueItem);

    if (!isExecutionState(generator))
        asyncGeneratorResumeNext(globalObject, generator);

    return promise;
}

JSC_DEFINE_HOST_FUNCTION(asyncGeneratorProtoFuncNext, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue value = callFrame->argument(0);
    return JSValue::encode(asyncGeneratorEnqueue(globalObject, callFrame->thisValue(), value, static_cast<int32_t>(JSGenerator::ResumeMode::NormalMode)));
}

JSC_DEFINE_HOST_FUNCTION(asyncGeneratorProtoFuncReturn, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue value = callFrame->argument(0);
    return JSValue::encode(asyncGeneratorEnqueue(globalObject, callFrame->thisValue(), value, static_cast<int32_t>(JSGenerator::ResumeMode::ReturnMode)));
}

JSC_DEFINE_HOST_FUNCTION(asyncGeneratorProtoFuncThrow, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue exception = callFrame->argument(0);
    return JSValue::encode(asyncGeneratorEnqueue(globalObject, callFrame->thisValue(), exception, static_cast<int32_t>(JSGenerator::ResumeMode::ThrowMode)));
}

void AsyncGeneratorPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("next"_s, asyncGeneratorProtoFuncNext, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("return"_s, asyncGeneratorProtoFuncReturn, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION("throw"_s, asyncGeneratorProtoFuncThrow, static_cast<unsigned>(PropertyAttribute::DontEnum), 1, ImplementationVisibility::Public);

    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
