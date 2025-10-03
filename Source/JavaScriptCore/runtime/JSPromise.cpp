/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JSPromise.h"

#include "BuiltinNames.h"
#include "DeferredWorkTimer.h"
#include "ErrorInstance.h"
#include "GlobalObjectMethodTable.h"
#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "JSInternalFieldObjectImplInlines.h"
#include "JSInternalPromisePrototype.h"
#include "JSPromiseAllContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "ObjectConstructor.h"

namespace JSC {

const ClassInfo JSPromise::s_info = { "Promise"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

JSPromise* JSPromise::createWithInitialValues(VM& vm, Structure* structure)
{
    return create(vm, structure);
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSPromiseType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void JSPromise::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).set(vm, this, values[index]);
}

template<typename Visitor>
void JSPromise::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromise*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSPromise);

JSValue JSPromise::createNewPromiseCapability(JSGlobalObject* globalObject, JSObject* constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });

    auto* capability = constructEmptyObject(globalObject);
    capability->putDirect(vm, vm.propertyNames->resolve, resolve);
    capability->putDirect(vm, vm.propertyNames->reject, reject);
    capability->putDirect(vm, vm.propertyNames->promise, promise);

    return capability;
}

std::tuple<JSObject*, JSObject*, JSObject*> JSPromise::newPromiseCapability(JSGlobalObject* globalObject, JSObject* constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // FIXME: Add optimizations here.
    // if (constructor == globalObject->promiseConstructor())

    auto* executor = JSFunctionWithFields::create(vm, globalObject, vm.promiseCapabilityExecutorExecutable(), 2, nullString());

    MarkedArgumentBuffer args;
    args.append(executor);
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "Species construction did not get a valid constructor"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue resolve = executor->fields()[0].get();
    JSValue reject = executor->fields()[1].get();
    if (!resolve.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a resolve function"_s);
        return { };
    }

    if (!reject.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a reject function"_s);
        return { };
    }

    return { newObject, asObject(resolve), asObject(reject) };
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto [ promiseCapability, resolveCapability, rejectCapability ] = newPromiseCapability(globalObject, promiseConstructor);
    RETURN_IF_EXCEPTION(scope, { });
    auto* promise = jsDynamicCast<JSPromise*>(promiseCapability);
    auto* resolve = jsDynamicCast<JSFunction*>(resolveCapability);
    auto* reject  = jsDynamicCast<JSFunction*>(rejectCapability);
    if (promise && resolve && reject)
        return DeferredData { promise, resolve, reject };

    throwTypeError(globalObject, scope, "constructor is producing a bad value"_s);
    return { };
}

JSPromise* JSPromise::resolvedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* function = globalObject->promiseResolveFunction();
    auto callData = JSC::getCallData(function);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(value);
    ASSERT(!arguments.hasOverflowed());
    auto result = call(globalObject, function, callData, globalObject->promiseConstructor(), arguments);
    RETURN_IF_EXCEPTION(scope, nullptr);
    ASSERT(result.inherits<JSPromise>());
    return jsCast<JSPromise*>(result);
}

JSPromise* JSPromise::rejectedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->reject(globalObject, value);
    return promise;
}

void JSPromise::resolve(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        resolvePromise(globalObject, value);
    }
}

void JSPromise::reject(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        rejectPromise(globalObject, value);
    }
}

void JSPromise::fulfill(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    uint32_t flags = this->flags();
    ASSERT(!value.inherits<Exception>());
    if (!(flags & isFirstResolvingFunctionCalledFlag)) {
        internalField(Field::Flags).set(vm, this, jsNumber(flags | isFirstResolvingFunctionCalledFlag));
        fulfillPromise(globalObject, value);
    }
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, JSValue value)
{
    // Setting isHandledFlag before calling reject since this removes round-trip between JSC and PromiseRejectionTracker, and it does not show an user-observable behavior.
    if (!(flags() & isFirstResolvingFunctionCalledFlag)) {
        markAsHandled();
        reject(lexicalGlobalObject, value);
    }
}

void JSPromise::reject(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    reject(lexicalGlobalObject, reason->value());
}

void JSPromise::rejectAsHandled(JSGlobalObject* lexicalGlobalObject, Exception* reason)
{
    rejectAsHandled(lexicalGlobalObject, reason->value());
}

JSPromise* JSPromise::rejectWithCaughtException(JSGlobalObject* globalObject, ThrowScope& scope)
{
    VM& vm = globalObject->vm();
    Exception* exception = scope.exception();
    ASSERT(exception);
    if (vm.isTerminationException(exception)) [[unlikely]] {
        scope.release();
        return this;
    }
    scope.clearException();
    scope.release();
    reject(globalObject, exception->value());
    return this;
}

void JSPromise::performPromiseThen(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability, JSValue context)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!onFulfilled.isCallable())
        onFulfilled = globalObject->promiseEmptyOnFulfilledFunction();

    if (!onRejected.isCallable())
        onRejected = globalObject->promiseEmptyOnRejectedFunction();

    JSValue reactionsOrResult = this->reactionsOrResult();
    switch (status()) {
    case JSPromise::Status::Pending: {
        auto* reaction = JSPromiseReaction::create(vm, globalObject->promiseReactionStructure(), promiseOrCapability, onFulfilled, onRejected, context, reactionsOrResult);
        setReactionsOrResult(vm, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        if (!isHandled()) {
            if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
                globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
                RETURN_IF_EXCEPTION(scope, void());
            }
        }
        scope.release();
        globalObject->queueMicrotask(globalObject->promiseReactionJobFunction(), promiseOrCapability, onRejected, reactionsOrResult, context);
        break;
    }
    case JSPromise::Status::Fulfilled: {
        scope.release();
        globalObject->queueMicrotask(globalObject->promiseReactionJobFunction(), promiseOrCapability, onFulfilled, reactionsOrResult, context);
        break;
    }
    }
    markAsHandled();
}

void JSPromise::rejectPromise(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Rejected)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);

    if (!isHandled()) {
        if (globalObject->globalObjectMethodTable()->promiseRejectionTracker) {
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Reject);
            RETURN_IF_EXCEPTION(scope, void());
        } else
            vm.promiseRejected(this);
    }

    RELEASE_AND_RETURN(scope, triggerPromiseReactions(globalObject, Status::Rejected, reactions, argument));
}

void JSPromise::fulfillPromise(JSGlobalObject* globalObject, JSValue argument)
{
    VM& vm = globalObject->vm();

    ASSERT(status() == Status::Pending);
    uint32_t flags = this->flags();
    auto* reactions = jsDynamicCast<JSPromiseReaction*>(this->reactionsOrResult());
    internalField(Field::Flags).set(vm, this, jsNumber(flags | static_cast<uint32_t>(Status::Fulfilled)));
    internalField(Field::ReactionsOrResult).set(vm, this, argument);
    triggerPromiseReactions(globalObject, Status::Fulfilled, reactions, argument);
}

void JSPromise::resolvePromise(JSGlobalObject* globalObject, JSValue resolution)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (resolution == this) [[unlikely]] {
        Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
        auto* error = ErrorInstance::create(vm, errorStructure, "Cannot resolve a promise with itself"_s, jsUndefined(), nullptr, TypeNothing, ErrorType::TypeError, false);
        return rejectPromise(globalObject, error);
    }

    if (!resolution.isObject())
        return fulfillPromise(globalObject, resolution);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(jsNumber(static_cast<int32_t>(InternalMicrotask::PromiseResolveThenableJobFast)), resolutionObject, this, jsUndefined(), jsUndefined());
    }

    JSValue then = resolutionObject->get(globalObject, vm.propertyNames->then);
    if (scope.exception()) [[unlikely]] {
        JSValue error = scope.exception()->value();
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
        return rejectPromise(globalObject, error);
    }

    if (!then.isCallable()) [[likely]]
        return fulfillPromise(globalObject, resolutionObject);

    auto [ resolve, reject ] = createResolvingFunctions(vm, globalObject);
    return globalObject->queueMicrotask(globalObject->promiseResolveThenableJobFunction(), resolutionObject, then, resolve, reject);
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->fields()[1].get());
    if (other) {
        callee->fields()[1].set(vm, callee, jsNull());
        other->fields()[1].set(vm, callee, jsNull());
    }

    auto* promise = jsCast<JSPromise*>(callee->fields()[0].get());
    JSValue argument = callFrame->argument(0);

    promise->resolvePromise(globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->fields()[1].get());
    if (other) {
        callee->fields()[1].set(vm, callee, jsNull());
        other->fields()[1].set(vm, callee, jsNull());
    }

    auto* promise = jsCast<JSPromise*>(callee->fields()[0].get());
    JSValue argument = callFrame->argument(0);

    promise->rejectPromise(globalObject, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolveWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->fields()[1].get());
    if (other) {
        callee->fields()[1].set(vm, callee, jsNull());
        other->fields()[1].set(vm, callee, jsNull());
    }

    auto* context = jsCast<JSPromiseAllContext*>(callee->fields()[0].get());
    JSValue argument = callFrame->argument(0);

    JSPromise::resolveWithoutPromise(globalObject, argument, context->values(), context->remainingElementsCount(), context->index());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionRejectWithoutPromise, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    auto* other = jsDynamicCast<JSFunctionWithFields*>(callee->fields()[1].get());
    if (other) {
        callee->fields()[1].set(vm, callee, jsNull());
        other->fields()[1].set(vm, callee, jsNull());
    }

    auto* context = jsCast<JSPromiseAllContext*>(callee->fields()[0].get());
    JSValue argument = callFrame->argument(0);

    JSPromise::rejectWithoutPromise(globalObject, argument, context->values(), context->remainingElementsCount(), context->index());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseCapabilityExecutor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = jsCast<JSFunctionWithFields*>(callFrame->jsCallee());
    if (callee->fields()[0].get())
        return throwVMTypeError(globalObject, scope, "resolve function is already set"_s);

    if (callee->fields()[1].get())
        return throwVMTypeError(globalObject, scope, "reject function is already set"_s);

    callee->fields()[0].set(vm, callee, callFrame->argument(0));
    callee->fields()[1].set(vm, callee, callFrame->argument(1));

    return JSValue::encode(jsUndefined());
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctions(VM& vm, JSGlobalObject* globalObject)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectExecutable(), 1, nullString());

    resolve->fields()[0].set(vm, resolve, this);
    resolve->fields()[1].set(vm, resolve, reject);

    reject->fields()[0].set(vm, reject, this);
    reject->fields()[1].set(vm, reject, resolve);

    return std::tuple { resolve, reject };
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctionsWithoutPromise(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveWithoutPromiseExecutable(), 1, nullString());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectWithoutPromiseExecutable(), 1, nullString());

    auto* all = JSPromiseAllContext::create(vm, globalObject->promiseAllContextStructure(), jsNull(), onFulfilled, onRejected, context);

    resolve->fields()[0].set(vm, resolve, all);
    resolve->fields()[1].set(vm, resolve, reject);

    reject->fields()[0].set(vm, reject, all);
    reject->fields()[1].set(vm, reject, resolve);

    return std::tuple { resolve, reject };
}

void JSPromise::triggerPromiseReactions(JSGlobalObject* globalObject, Status status, JSPromiseReaction* head, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!head)
        return;

    // Reverse the order of singly-linked-list.
    JSValue previous = jsUndefined();
    {
        auto* current = head;
        while (current) {
            auto* next = jsDynamicCast<JSPromiseReaction*>(current->next());
            current->setNext(vm, previous);
            previous = current;
            current = next;
        }
    }
    head = jsCast<JSPromiseReaction*>(previous);

    JSFunction* function = globalObject->promiseReactionJobFunction();
    bool isResolved = status == JSPromise::Status::Fulfilled;
    auto* current = head;
    while (current) {
        JSValue promise = current->promise();
        JSValue handler = isResolved ? current->onFulfilled() : current->onRejected();
        JSValue context = current->context();
        current = jsDynamicCast<JSPromiseReaction*>(current->next());

        globalObject->queueMicrotask(function, promise, handler, argument, handler.isUndefinedOrNull() ? jsNumber(static_cast<int32_t>(status)) : context);
        RETURN_IF_EXCEPTION(scope, void());
    }
}

void JSPromise::resolveWithoutPromise(JSGlobalObject* globalObject, JSValue resolution, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

    if (!resolution.isObject())
        return fulfillWithoutPromise(globalObject, resolution, onFulfilled, onRejected, context);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = jsCast<JSPromise*>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(jsNumber(static_cast<int32_t>(InternalMicrotask::PromiseResolveThenableJobWithoutPromiseFast)), resolutionObject, onFulfilled, onRejected, context);
    }

    JSValue then = resolutionObject->get(globalObject, vm.propertyNames->then);
    if (scope.exception()) [[unlikely]] {
        JSValue error = scope.exception()->value();
        if (!scope.clearExceptionExceptTermination()) [[unlikely]]
            return;
        return rejectWithoutPromise(globalObject, error, onFulfilled, onRejected, context);
    }

    if (!then.isCallable()) [[likely]]
        return fulfillWithoutPromise(globalObject, resolution, onFulfilled, onRejected, context);

    auto [ resolve, reject ] = createResolvingFunctionsWithoutPromise(vm, globalObject, onFulfilled, onRejected, context);
    return globalObject->queueMicrotask(globalObject->promiseResolveThenableJobFunction(), resolutionObject, then, resolve, reject);
}

void JSPromise::rejectWithoutPromise(JSGlobalObject* globalObject, JSValue value, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    UNUSED_PARAM(onFulfilled);
    globalObject->queueMicrotask(globalObject->promiseReactionJobWithoutPromiseFunction(), onRejected, value, context, jsUndefined());
}

void JSPromise::fulfillWithoutPromise(JSGlobalObject* globalObject, JSValue value, JSValue onFulfilled, JSValue onRejected, JSValue context)
{
    UNUSED_PARAM(onRejected);
    globalObject->queueMicrotask(globalObject->promiseReactionJobWithoutPromiseFunction(), onFulfilled, value, context, jsUndefined());
}

bool JSPromise::isThenFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->globalObject();
    Structure* structure = this->structure();
    // We do not allow overriding `then` in InternalPromise.
    if (inherits<JSInternalPromise>())
        return true;

    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    if (structure == globalObject->promiseStructure())
        return true;

    if (getPrototypeDirect() != globalObject->promisePrototype())
        return false;

    VM& vm = globalObject->vm();
    if (getDirectOffset(vm, vm.propertyNames->then) != invalidOffset)
        return false;

    return true;
}

JSObject* promiseSpeciesConstructor(JSGlobalObject* globalObject, JSObject* thisObject)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* promise = jsDynamicCast<JSPromise*>(thisObject)) [[likely]] {
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return globalObject->promiseConstructor();
    }

    JSValue constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefined())
        return globalObject->promiseConstructor();

    if (!constructor.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, "|this|.constructor is not an Object or undefined"_s);
        return { };
    }

    constructor = asObject(constructor)->get(globalObject, vm.propertyNames->speciesSymbol);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefinedOrNull())
        return globalObject->promiseConstructor();

    if (constructor.isConstructor()) [[likely]]
        return asObject(constructor);

    throwTypeError(globalObject, scope, "|this|.constructor[Symbol.species] is not a constructor"_s);
    return { };
}


} // namespace JSC
