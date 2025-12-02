/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "JSGenerator.h"
#include "JSInternalFieldObjectImpl.h"

namespace JSC {

class JSAsyncGenerator final : public JSInternalFieldObjectImpl<7> {
public:
    using Base = JSInternalFieldObjectImpl<7>;

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.asyncGeneratorSpace<mode>();
    }

    enum class AsyncGeneratorState : int32_t {
        Completed = -1,
        Executing = -2,
        SuspendedStart = -3,
        SuspendedYield = -4,
        AwaitingReturn = -5,
    };
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Completed) == static_cast<int32_t>(JSGenerator::State::Completed));
    static_assert(static_cast<int32_t>(AsyncGeneratorState::Executing) == static_cast<int32_t>(JSGenerator::State::Executing));

    enum class AsyncGeneratorSuspendReason : int32_t {
        None = 0,
        Yield = -1,
        Await = -2
    };

    enum class Field : uint32_t {
        State = 0,
        Next,
        This,
        Frame,
        SuspendReason,
        QueueFirst,
        QueueLast,
    };
    static_assert(numberOfInternalFields == 7);
    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(static_cast<int32_t>(AsyncGeneratorState::SuspendedStart)),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsNumber(static_cast<int32_t>(AsyncGeneratorSuspendReason::None)),
            jsNull(),
            jsNull(),
        } };
    }

    static JSAsyncGenerator* create(VM&, Structure*);
    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    int32_t state() const
    {
        return Base::internalField(static_cast<unsigned>(Field::State)).get().asInt32AsAnyInt();
    }

    void setState(VM& vm, int32_t state)
    {
        Base::internalField(static_cast<unsigned>(Field::State)).set(vm, this, jsNumber(state));
    }

    int32_t suspendReason() const
    {
        return Base::internalField(static_cast<unsigned>(Field::SuspendReason)).get().asInt32AsAnyInt();
    }

    void setSuspendReason(VM& vm, int32_t reason)
    {
        Base::internalField(static_cast<unsigned>(Field::SuspendReason)).set(vm, this, jsNumber(reason));
    }

    JSValue next() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Next)).get();
    }

    JSValue thisValue() const
    {
        return Base::internalField(static_cast<unsigned>(Field::This)).get();
    }

    JSValue frame() const
    {
        return Base::internalField(static_cast<unsigned>(Field::Frame)).get();
    }

    JSValue queueFirst() const
    {
        return Base::internalField(static_cast<unsigned>(Field::QueueFirst)).get();
    }

    void setQueueFirst(VM& vm, JSValue value)
    {
        Base::internalField(static_cast<unsigned>(Field::QueueFirst)).set(vm, this, value);
    }

    JSValue queueLast() const
    {
        return Base::internalField(static_cast<unsigned>(Field::QueueLast)).get();
    }

    void setQueueLast(VM& vm, JSValue value)
    {
        Base::internalField(static_cast<unsigned>(Field::QueueLast)).set(vm, this, value);
    }

    DECLARE_EXPORT_INFO;

    DECLARE_VISIT_CHILDREN;

private:
    JSAsyncGenerator(VM&, Structure*);
    void finishCreation(VM&);
};

} // namespace JSC
