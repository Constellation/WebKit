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

#include <initializer_list>
#include <wtf/Forward.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/OrderedHashTable.h>

namespace WTF {

template<typename ValueArg, typename HashArg, typename TraitsArg, typename Malloc>
class OrderedHashSet final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OrderedHashSet);
private:
    using HashFunctions = HashArg;
    using ValueTraits = TraitsArg;
    using TakeType = typename ValueTraits::TakeType;

public:
    using ValueType = typename ValueTraits::TraitType;

private:
    using HashTableType = OrderedHashTable<ValueType, ValueType, IdentityExtractor, HashFunctions, ValueTraits, ValueTraits, Malloc>;

public:
    using iterator = OrderedHashTableConstIterator<HashTableType, ValueType>;
    using const_iterator = OrderedHashTableConstIterator<HashTableType, ValueType>;

    struct AddResult {
        const_iterator iterator;
        bool isNewEntry;
    };

    OrderedHashSet() = default;

    OrderedHashSet(std::initializer_list<ValueArg> initializerList)
    {
        if (!initializerList.size())
            return;
        reserveInitialCapacity(initializerList.size());
        for (auto&& value : initializerList)
            add(std::forward<decltype(value)>(value));
    }

    template<typename ContainerType>
    explicit OrderedHashSet(ContainerType&& container)
    {
        if (!container.size())
            return;
        reserveInitialCapacity(container.size());
        for (auto&& value : std::forward<ContainerType>(container))
            add(std::forward<decltype(value)>(value));
    }

    void swap(OrderedHashSet& other) { m_impl.swap(other.m_impl); }

    unsigned size() const { return m_impl.size(); }
    unsigned capacity() const { return m_impl.capacity(); }
    bool isEmpty() const { return m_impl.isEmpty(); }

    void reserveInitialCapacity(unsigned keyCount) { m_impl.reserveInitialCapacity(keyCount); }

    iterator begin() const LIFETIME_BOUND { return m_impl.begin(); }
    iterator end() const LIFETIME_BOUND { return m_impl.end(); }

    iterator find(const ValueType& value) const LIFETIME_BOUND { return m_impl.find(value); }
    bool contains(const ValueType& value) const { return m_impl.contains(value); }

    template<typename HashTranslator, typename T>
    iterator find(const T& value) const LIFETIME_BOUND
    {
        return m_impl.template find<HashTranslator>(value);
    }

    template<typename HashTranslator, typename T>
    bool contains(const T& value) const
    {
        return m_impl.template contains<HashTranslator>(value);
    }

    AddResult add(const ValueType& value) LIFETIME_BOUND
    {
        auto result = m_impl.add(value, [&]() ALWAYS_INLINE_LAMBDA -> ValueType { return value; });
        return { result.iterator, result.isNewEntry };
    }

    AddResult add(ValueType&& value) LIFETIME_BOUND
    {
        auto movedValue = WTF::move(value);
        auto result = m_impl.add(movedValue, [&]() ALWAYS_INLINE_LAMBDA -> ValueType { return WTF::move(movedValue); });
        return { result.iterator, result.isNewEntry };
    }

    void addVoid(const ValueType& value) { add(value); }
    void addVoid(ValueType&& value) { add(WTF::move(value)); }

    template<typename ContainerType>
    bool addAll(ContainerType&& container)
    {
        bool changed = false;
        for (auto&& item : std::forward<ContainerType>(container))
            changed |= add(std::forward<decltype(item)>(item)).isNewEntry;
        return changed;
    }

    bool remove(const ValueType& value)
    {
        return remove(find(value));
    }

    bool remove(iterator it)
    {
        if (it == end())
            return false;
        m_impl.remove(it);
        return true;
    }

    bool removeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto& functor)
    {
        return m_impl.removeIf(functor);
    }

    void clear() { m_impl.clear(); }

    TakeType take(const ValueType& value)
    {
        return take(find(value));
    }

    TakeType take(iterator it)
    {
        if (it == end())
            return ValueTraits::take(ValueTraits::emptyValue());
        auto result = ValueTraits::take(WTF::move(const_cast<ValueType&>(*it)));
        remove(it);
        return result;
    }

    TakeType takeAny() { return take(begin()); }

    static bool isValidValue(const ValueType& value)
    {
        if (ValueTraits::isDeletedValue(value))
            return false;
        if (HashFunctions::safeToCompareToEmptyOrDeleted) {
            if (value == ValueTraits::emptyValue())
                return false;
        } else {
            if (isHashTraitsEmptyValue<ValueTraits>(value))
                return false;
        }
        return true;
    }

private:
    HashTableType m_impl;
};

} // namespace WTF

using WTF::OrderedHashSet;
