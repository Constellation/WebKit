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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <utility>
#include <wtf/Compiler.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/HashTraits.h>
#include <wtf/StdLibExtras.h>

namespace WTF {

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class OrderedHashTable;

template<typename TableType, typename ValueType>
class OrderedHashTableIterator;

template<typename TableType, typename ValueType>
class OrderedHashTableConstIterator;

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class OrderedHashTable final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(OrderedHashTable);
public:
    using KeyType = Key;
    using ValueType = Value;

    struct Entry {
        Value value;
        uint32_t chain;
    };

    struct AddResult {
        using IteratorType = OrderedHashTableIterator<OrderedHashTable, ValueType>;
        IteratorType iterator;
        bool isNewEntry;
    };

    using iterator = OrderedHashTableIterator<OrderedHashTable, ValueType>;
    using const_iterator = OrderedHashTableConstIterator<OrderedHashTable, ValueType>;

    OrderedHashTable() = default;

    OrderedHashTable(const OrderedHashTable& other)
    {
        if (other.m_liveCount) {
            initializeBuckets(other.m_bucketCount);
            allocateEntries(other.m_entriesCapacity);
            for (uint32_t i = 0; i < other.m_entriesLength; ++i) {
                if (!isDeletedEntry(other.m_entries[i])) {
                    auto& newEntry = m_entries[m_entriesLength];
                    new (NotNull, std::addressof(newEntry.value)) ValueType(other.m_entries[i].value);
                    insertIntoBuckets(m_entriesLength);
                    ++m_entriesLength;
                    ++m_liveCount;
                }
            }
        }
    }

    OrderedHashTable(OrderedHashTable&& other)
        : m_buckets(std::exchange(other.m_buckets, nullptr))
        , m_entries(std::exchange(other.m_entries, nullptr))
        , m_bucketCount(std::exchange(other.m_bucketCount, 0))
        , m_entriesCapacity(std::exchange(other.m_entriesCapacity, 0))
        , m_entriesLength(std::exchange(other.m_entriesLength, 0))
        , m_liveCount(std::exchange(other.m_liveCount, 0))
        , m_deletedCount(std::exchange(other.m_deletedCount, 0))
    {
    }

    OrderedHashTable& operator=(const OrderedHashTable& other)
    {
        if (this != &other) {
            OrderedHashTable tmp(other);
            swap(tmp);
        }
        return *this;
    }

    OrderedHashTable& operator=(OrderedHashTable&& other)
    {
        if (this != &other) {
            deallocateAll();
            m_buckets = std::exchange(other.m_buckets, nullptr);
            m_entries = std::exchange(other.m_entries, nullptr);
            m_bucketCount = std::exchange(other.m_bucketCount, 0);
            m_entriesCapacity = std::exchange(other.m_entriesCapacity, 0);
            m_entriesLength = std::exchange(other.m_entriesLength, 0);
            m_liveCount = std::exchange(other.m_liveCount, 0);
            m_deletedCount = std::exchange(other.m_deletedCount, 0);
        }
        return *this;
    }

    ~OrderedHashTable()
    {
        deallocateAll();
    }

    void swap(OrderedHashTable& other)
    {
        std::swap(m_buckets, other.m_buckets);
        std::swap(m_entries, other.m_entries);
        std::swap(m_bucketCount, other.m_bucketCount);
        std::swap(m_entriesCapacity, other.m_entriesCapacity);
        std::swap(m_entriesLength, other.m_entriesLength);
        std::swap(m_liveCount, other.m_liveCount);
        std::swap(m_deletedCount, other.m_deletedCount);
    }

    unsigned size() const { return m_liveCount; }
    unsigned capacity() const { return m_entriesCapacity; }
    bool isEmpty() const { return !m_liveCount; }

    iterator begin() LIFETIME_BOUND
    {
        return iterator(this, firstLiveIndex());
    }

    iterator end() LIFETIME_BOUND
    {
        return iterator(this, m_entriesLength);
    }

    const_iterator begin() const LIFETIME_BOUND
    {
        return const_iterator(this, firstLiveIndex());
    }

    const_iterator end() const LIFETIME_BOUND
    {
        return const_iterator(this, m_entriesLength);
    }

    ValueType* lookup(const KeyType& key)
    {
        if (!m_buckets)
            return nullptr;
        uint32_t bucketIndex = bucketForKey(key);
        uint32_t idx = m_buckets[bucketIndex];
        while (idx != emptyBucket) {
            auto& entry = m_entries[idx];
            if (HashFunctions::equal(Extractor::extract(entry.value), key))
                return &entry.value;
            idx = entry.chain;
        }
        return nullptr;
    }

    const ValueType* lookup(const KeyType& key) const
    {
        return const_cast<OrderedHashTable*>(this)->lookup(key);
    }

    template<typename HashTranslator, typename T>
    ValueType* lookup(const T& key)
    {
        if (!m_buckets)
            return nullptr;
        unsigned hash = HashTranslator::hash(key);
        uint32_t bucketIndex = hash & (m_bucketCount - 1);
        uint32_t idx = m_buckets[bucketIndex];
        while (idx != emptyBucket) {
            auto& entry = m_entries[idx];
            if (HashTranslator::equal(Extractor::extract(entry.value), key))
                return &entry.value;
            idx = entry.chain;
        }
        return nullptr;
    }

    template<typename HashTranslator, typename T>
    const ValueType* lookup(const T& key) const
    {
        return const_cast<OrderedHashTable*>(this)->template lookup<HashTranslator>(key);
    }

    iterator find(const KeyType& key) LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        uint32_t bucketIndex = bucketForKey(key);
        uint32_t idx = m_buckets[bucketIndex];
        while (idx != emptyBucket) {
            auto& entry = m_entries[idx];
            if (HashFunctions::equal(Extractor::extract(entry.value), key))
                return iterator(this, idx);
            idx = entry.chain;
        }
        return end();
    }

    const_iterator find(const KeyType& key) const LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        uint32_t bucketIndex = bucketForKey(key);
        uint32_t idx = m_buckets[bucketIndex];
        while (idx != emptyBucket) {
            auto& entry = m_entries[idx];
            if (HashFunctions::equal(Extractor::extract(entry.value), key))
                return const_iterator(this, idx);
            idx = entry.chain;
        }
        return end();
    }

    template<typename HashTranslator, typename T>
    iterator find(const T& key) LIFETIME_BOUND
    {
        if (!m_buckets)
            return end();
        unsigned hash = HashTranslator::hash(key);
        uint32_t bucketIndex = hash & (m_bucketCount - 1);
        uint32_t idx = m_buckets[bucketIndex];
        while (idx != emptyBucket) {
            auto& entry = m_entries[idx];
            if (HashTranslator::equal(Extractor::extract(entry.value), key))
                return iterator(this, idx);
            idx = entry.chain;
        }
        return end();
    }

    template<typename HashTranslator, typename T>
    const_iterator find(const T& key) const LIFETIME_BOUND
    {
        return const_cast<OrderedHashTable*>(this)->template find<HashTranslator>(key);
    }

    bool contains(const KeyType& key) const
    {
        return lookup(key) != nullptr;
    }

    template<typename HashTranslator, typename T>
    bool contains(const T& key) const
    {
        return const_cast<OrderedHashTable*>(this)->template lookup<HashTranslator>(key) != nullptr;
    }

    AddResult add(const KeyType& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd(key, valueFunctor);
    }

    AddResult add(KeyType&& key, NOESCAPE const auto& valueFunctor)
    {
        return internalAdd(WTF::move(key), valueFunctor);
    }

    template<typename HashTranslator, typename K>
    AddResult add(K&& key, NOESCAPE const auto& valueFunctor)
    {
        if (m_buckets) {
            unsigned hash = HashTranslator::hash(key);
            uint32_t bucketIndex = hash & (m_bucketCount - 1);
            uint32_t idx = m_buckets[bucketIndex];
            while (idx != emptyBucket) {
                auto& entry = m_entries[idx];
                if (HashTranslator::equal(Extractor::extract(entry.value), key))
                    return { iterator(this, idx), false };
                idx = entry.chain;
            }
        }

        rehashIfNeeded();

        auto& entry = m_entries[m_entriesLength];
        HashTranslator::translate(entry.value, std::forward<K>(key), valueFunctor);
        insertIntoBuckets(m_entriesLength);
        auto result = AddResult { iterator(this, m_entriesLength), true };
        ++m_entriesLength;
        ++m_liveCount;
        return result;
    }

    void remove(iterator it)
    {
        ASSERT(it.m_table == this);
        ASSERT(it.m_index < m_entriesLength);
        removeEntryAtIndex(it.m_index);
    }

    void remove(const_iterator it)
    {
        ASSERT(it.m_table == this);
        ASSERT(it.m_index < m_entriesLength);
        removeEntryAtIndex(it.m_index);
    }

    void remove(const KeyType& key)
    {
        auto it = find(key);
        if (it != end())
            remove(it);
    }

    bool removeIf(NOESCAPE const auto& functor)
    {
        bool changed = false;
        for (uint32_t i = 0; i < m_entriesLength; ++i) {
            if (!isDeletedEntry(m_entries[i]) && functor(m_entries[i].value)) {
                removeEntryAtIndex(i);
                changed = true;
            }
        }
        return changed;
    }

    void clear()
    {
        deallocateAll();
        m_buckets = nullptr;
        m_entries = nullptr;
        m_bucketCount = 0;
        m_entriesCapacity = 0;
        m_entriesLength = 0;
        m_liveCount = 0;
        m_deletedCount = 0;
    }

    void reserveInitialCapacity(unsigned keyCount)
    {
        if (!keyCount)
            return;
        uint32_t newBucketCount = initialBucketCount;
        while (newBucketCount * maxLoad < keyCount)
            newBucketCount <<= 1;
        if (newBucketCount > m_bucketCount) {
            deallocateAll();
            initializeBuckets(newBucketCount);
            allocateEntries(newBucketCount * maxLoad);
        }
    }

    // Internal accessors used by iterators
    Entry* entries() { return m_entries; }
    const Entry* entries() const { return m_entries; }
    uint32_t entriesLength() const { return m_entriesLength; }

    bool isDeletedEntry(const Entry& entry) const
    {
        return KeyTraits::isDeletedValue(Extractor::extract(entry.value));
    }

private:
    static constexpr uint32_t emptyBucket = UINT32_MAX;
    static constexpr unsigned initialBucketCount = 4;
    static constexpr unsigned maxLoad = 2;

    uint32_t bucketForKey(const KeyType& key) const
    {
        return HashFunctions::hash(key) & (m_bucketCount - 1);
    }

    uint32_t bucketForEntry(uint32_t entryIndex) const
    {
        return HashFunctions::hash(Extractor::extract(m_entries[entryIndex].value)) & (m_bucketCount - 1);
    }

    void insertIntoBuckets(uint32_t entryIndex)
    {
        uint32_t bucketIndex = bucketForEntry(entryIndex);
        m_entries[entryIndex].chain = m_buckets[bucketIndex];
        m_buckets[bucketIndex] = entryIndex;
    }

    void initializeBuckets(uint32_t count)
    {
        m_bucketCount = count;
        m_buckets = static_cast<uint32_t*>(Malloc::malloc(count * sizeof(uint32_t)));
        std::fill_n(m_buckets, count, emptyBucket);
    }

    void allocateEntries(uint32_t cap)
    {
        m_entriesCapacity = cap;
        m_entries = static_cast<Entry*>(Malloc::malloc(cap * sizeof(Entry)));
    }

    void deallocateAll()
    {
        if (m_entries) {
            for (uint32_t i = 0; i < m_entriesLength; ++i) {
                if (!isDeletedEntry(m_entries[i]))
                    m_entries[i].value.~ValueType();
            }
            Malloc::free(m_entries);
        }
        if (m_buckets)
            Malloc::free(m_buckets);
    }

    template<typename K>
    AddResult internalAdd(K&& key, NOESCAPE const auto& valueFunctor)
    {
        if (m_buckets) {
            uint32_t bucketIndex = bucketForKey(key);
            uint32_t idx = m_buckets[bucketIndex];
            while (idx != emptyBucket) {
                auto& entry = m_entries[idx];
                if (HashFunctions::equal(Extractor::extract(entry.value), key))
                    return { iterator(this, idx), false };
                idx = entry.chain;
            }
        }

        rehashIfNeeded();

        auto& entry = m_entries[m_entriesLength];
        new (NotNull, std::addressof(entry.value)) ValueType(valueFunctor());
        insertIntoBuckets(m_entriesLength);
        auto result = AddResult { iterator(this, m_entriesLength), true };
        ++m_entriesLength;
        ++m_liveCount;
        return result;
    }

    void removeEntryAtIndex(uint32_t index)
    {
        ASSERT(index < m_entriesLength);
        ASSERT(!isDeletedEntry(m_entries[index]));

        // Remove from bucket chain
        uint32_t bucketIndex = bucketForEntry(index);
        uint32_t* prev = &m_buckets[bucketIndex];
        while (*prev != index) {
            ASSERT(*prev != emptyBucket);
            prev = &m_entries[*prev].chain;
        }
        *prev = m_entries[index].chain;

        // Destroy the value and mark as deleted
        hashTraitsDeleteBucket<Traits>(m_entries[index].value);

        --m_liveCount;
        ++m_deletedCount;

        shrinkIfNeeded();
    }

    void rehashIfNeeded()
    {
        if (!m_buckets) {
            initializeBuckets(initialBucketCount);
            allocateEntries(initialBucketCount * maxLoad);
            return;
        }

        if (m_entriesLength < m_entriesCapacity)
            return;

        // Table is full, need to either grow or compact
        if (m_liveCount >= m_entriesCapacity * 3 / 4)
            rehash(m_bucketCount << 1); // Grow
        else
            rehash(m_bucketCount); // Compact in place
    }

    void shrinkIfNeeded()
    {
        if (m_bucketCount <= initialBucketCount)
            return;
        if (m_liveCount >= m_entriesLength / 4)
            return;
        rehash(std::max<uint32_t>(m_bucketCount >> 1, initialBucketCount));
    }

    void rehash(uint32_t newBucketCount)
    {
        uint32_t* oldBuckets = m_buckets;
        Entry* oldEntries = m_entries;
        uint32_t oldLength = m_entriesLength;

        uint32_t newCapacity = newBucketCount * maxLoad;

        initializeBuckets(newBucketCount);
        allocateEntries(newCapacity);

        m_entriesLength = 0;
        m_liveCount = 0;
        m_deletedCount = 0;

        for (uint32_t i = 0; i < oldLength; ++i) {
            if (!KeyTraits::isDeletedValue(Extractor::extract(oldEntries[i].value))) {
                auto& newEntry = m_entries[m_entriesLength];
                new (NotNull, std::addressof(newEntry.value)) ValueType(WTF::move(oldEntries[i].value));
                oldEntries[i].value.~ValueType();
                insertIntoBuckets(m_entriesLength);
                ++m_entriesLength;
                ++m_liveCount;
            }
        }

        Malloc::free(oldEntries);
        Malloc::free(oldBuckets);
    }

    uint32_t firstLiveIndex() const
    {
        for (uint32_t i = 0; i < m_entriesLength; ++i) {
            if (!isDeletedEntry(m_entries[i]))
                return i;
        }
        return m_entriesLength;
    }

    uint32_t* m_buckets { nullptr };
    Entry* m_entries { nullptr };
    uint32_t m_bucketCount { 0 };
    uint32_t m_entriesCapacity { 0 };
    uint32_t m_entriesLength { 0 };
    uint32_t m_liveCount { 0 };
    uint32_t m_deletedCount { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = ValueType*;
    using reference = ValueType&;

public:
    OrderedHashTableIterator() = default;

    ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index].value;
    }

    ValueType& operator*() const { return *get(); }
    ValueType* operator->() const { return get(); }

    OrderedHashTableIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        ++m_index;
        skipDeleted();
        return *this;
    }

    friend bool operator==(const OrderedHashTableIterator& a, const OrderedHashTableIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;
    template<typename, typename> friend class OrderedHashTableConstIterator;
    // Give OrderedHashMap/OrderedHashSet access to m_index for remove(iterator)
    template<typename, typename, typename, typename, typename, typename, typename> friend class OrderedHashTable;

    OrderedHashTableIterator(TableType* table, uint32_t index)
        : m_table(table)
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index < m_table->entriesLength() && m_table->isDeletedEntry(m_table->entries()[m_index]))
            ++m_index;
    }

    TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

template<typename TableType, typename ValueType>
class OrderedHashTableConstIterator {
    using iterator_category = std::forward_iterator_tag;
    using value_type = ValueType;
    using difference_type = ptrdiff_t;
    using pointer = const ValueType*;
    using reference = const ValueType&;

public:
    OrderedHashTableConstIterator() = default;

    OrderedHashTableConstIterator(const OrderedHashTableIterator<std::remove_const_t<TableType>, ValueType>& other)
        : m_table(other.m_table)
        , m_index(other.m_index)
    {
    }

    const ValueType* get() const
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        return &m_table->entries()[m_index].value;
    }

    const ValueType& operator*() const { return *get(); }
    const ValueType* operator->() const { return get(); }

    OrderedHashTableConstIterator& operator++()
    {
        ASSERT(m_table);
        ASSERT(m_index < m_table->entriesLength());
        ++m_index;
        skipDeleted();
        return *this;
    }

    friend bool operator==(const OrderedHashTableConstIterator& a, const OrderedHashTableConstIterator& b)
    {
        return a.m_table == b.m_table && a.m_index == b.m_index;
    }

private:
    friend TableType;
    template<typename, typename, typename, typename, typename, typename, typename> friend class OrderedHashTable;

    OrderedHashTableConstIterator(const TableType* table, uint32_t index)
        : m_table(table)
        , m_index(index)
    {
        skipDeleted();
    }

    void skipDeleted()
    {
        while (m_index < m_table->entriesLength() && m_table->isDeletedEntry(m_table->entries()[m_index]))
            ++m_index;
    }

    const TableType* m_table { nullptr };
    uint32_t m_index { 0 };
};

} // namespace WTF
