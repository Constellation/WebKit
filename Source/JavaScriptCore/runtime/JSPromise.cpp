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
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"

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

JSValue JSPromise::createNewPromiseCapability(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSFunction* newPromiseCapabilityFunction = globalObject->newPromiseCapabilityFunction();
    auto callData = JSC::getCallData(newPromiseCapabilityFunction);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(promiseConstructor);
    ASSERT(!arguments.hasOverflowed());
    RELEASE_AND_RETURN(scope, call(globalObject, newPromiseCapabilityFunction, callData, jsUndefined(), arguments));
}

JSPromise::DeferredData JSPromise::convertCapabilityToDeferredData(JSGlobalObject* globalObject, JSValue promiseCapability)
{
    DeferredData result;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    result.promise = promiseCapability.getAs<JSPromise*>(globalObject, vm.propertyNames->builtinNames().promisePublicName());
    RETURN_IF_EXCEPTION(scope, { });
    result.resolve = promiseCapability.getAs<JSFunction*>(globalObject, vm.propertyNames->builtinNames().resolvePublicName());
    RETURN_IF_EXCEPTION(scope, { });
    result.reject = promiseCapability.getAs<JSFunction*>(globalObject, vm.propertyNames->builtinNames().rejectPublicName());
    RETURN_IF_EXCEPTION(scope, { });

    return result;
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSValue capability = createNewPromiseCapability(globalObject, promiseConstructor);
    RETURN_IF_EXCEPTION(scope, { });
    RELEASE_AND_RETURN(scope, convertCapabilityToDeferredData(globalObject, capability));
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

// Keep in sync with @rejectPromise in JS.
JSPromise* JSPromise::rejectedPromise(JSGlobalObject* globalObject, JSValue value)
{
    // Because we create a promise in this function, we know that no promise reactions are registered.
    // We can skip triggering them, which completely avoids calling JS functions.
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->internalField(Field::Flags).set(vm, promise, jsNumber(promise->flags() | isFirstResolvingFunctionCalledFlag | static_cast<unsigned>(Status::Rejected)));
    promise->internalField(Field::ReactionsOrResult).set(vm, promise, value);
    if (globalObject->globalObjectMethodTable()->promiseRejectionTracker)
        globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, promise, JSPromiseRejectionOperation::Reject);
    else
        vm.promiseRejected(promise);
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

void JSPromise::performPromiseThen(JSGlobalObject* globalObject, JSFunction* onFulFilled, JSFunction* onRejected, JSValue resultCapability)
{
    JSFunction* performPromiseThenFunction = globalObject->performPromiseThenFunction();
    auto callData = JSC::getCallData(performPromiseThenFunction);
    ASSERT(callData.type != CallData::Type::None);

    MarkedArgumentBuffer arguments;
    arguments.append(this);
    arguments.append(onFulFilled);
    arguments.append(onRejected);
    arguments.append(resultCapability);
    arguments.append(jsUndefined());
    ASSERT(!arguments.hasOverflowed());
    call(globalObject, performPromiseThenFunction, callData, jsUndefined(), arguments);
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

} // namespace JSC
