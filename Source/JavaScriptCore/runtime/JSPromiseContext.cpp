/*
 * Copyright (C) 2025 Codeblog CORP.
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
#include "JSPromiseContext.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSPromiseContext::s_info = { "PromiseContext"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromiseContext) };

JSPromiseContext* JSPromiseContext::createWithInitialValues(VM& vm, Structure* structure)
{
    auto values = initialValues();
    JSPromiseContext* context = new (NotNull, allocateCell<JSPromiseContext>(vm)) JSPromiseContext(vm, structure);
    context->finishCreation(vm, values[0], values[1], values[2], values[3]);
    return context;
}

JSPromiseContext* JSPromiseContext::create(VM& vm, Structure* structure, JSValue promise, JSValue values, JSValue remainingElementsCount, JSValue index)
{
    JSPromiseContext* result = new (NotNull, allocateCell<JSPromiseContext>(vm)) JSPromiseContext(vm, structure);
    result->finishCreation(vm, promise, values, remainingElementsCount, index);
    return result;
}

void JSPromiseContext::finishCreation(VM& vm, JSValue promise, JSValue values, JSValue remainingElementsCount, JSValue index)
{
    Base::finishCreation(vm);
    this->setPromise(vm, promise);
    this->setValues(vm, values);
    this->setRemainingElementsCount(vm, remainingElementsCount);
    this->setIndex(vm, index);
}

template<typename Visitor>
void JSPromiseContext::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromiseContext*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSPromiseContext);

JSC_DEFINE_HOST_FUNCTION(promiseContextPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(JSPromiseContext::create(globalObject->vm(), globalObject->promiseContextStructure(), callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1), callFrame->uncheckedArgument(2), callFrame->uncheckedArgument(3)));
}

} // namespace JSC
