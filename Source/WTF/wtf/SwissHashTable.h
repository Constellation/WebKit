/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Inspired by Google's SwissTable / Abseil raw_hash_set design.
 * See: https://abseil.io/about/design/swisstables
 *
 * Copyright 2018 The Abseil Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

#include <wtf/AlignedStorage.h>
#include <wtf/HashTable.h>
#include <wtf/MathExtras.h>
#include <wtf/SIMDHelpers.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// --- Control byte encoding ---
// Full slot: stores H2 (top 7 bits of hash), value 0..127 (MSB=0)
// Special markers have MSB set (negative as int8_t).
enum class SwissCtrl : int8_t {
    kEmpty    = -128, // 0x80
    kDeleted  = -2,   // 0xFE
    kSentinel = -1,   // 0xFF — terminates iteration
};

ALWAYS_INLINE bool swissCtrlIsFull(SwissCtrl ctrl) { return static_cast<int8_t>(ctrl) >= 0; }
ALWAYS_INLINE bool swissCtrlIsEmpty(SwissCtrl ctrl) { return ctrl == SwissCtrl::kEmpty; }
ALWAYS_INLINE bool swissCtrlIsDeleted(SwissCtrl ctrl) { return ctrl == SwissCtrl::kDeleted; }
ALWAYS_INLINE bool swissCtrlIsEmptyOrDeleted(SwissCtrl ctrl) { return static_cast<int8_t>(ctrl) < static_cast<int8_t>(SwissCtrl::kSentinel); }

// --- H1/H2 hash splitting ---
ALWAYS_INLINE unsigned swissH1(unsigned hash, unsigned mask) { return hash & mask; }
ALWAYS_INLINE uint8_t swissH2(unsigned hash) { return static_cast<uint8_t>(hash >> 25); }

// --- BitMask for iterating SIMD match results ---
class SwissBitMask {
public:
    ALWAYS_INLINE explicit SwissBitMask(uint32_t mask)
        : m_mask(mask)
    {
    }

    ALWAYS_INLINE explicit operator bool() const { return m_mask != 0; }
    ALWAYS_INLINE unsigned lowestSetBit() const { return std::countr_zero(m_mask); }
    ALWAYS_INLINE SwissBitMask& operator++()
    {
        m_mask &= (m_mask - 1);
        return *this;
    }

private:
    uint32_t m_mask;
};

// --- SIMD Group: operates on 16 control bytes at once ---
struct SwissGroup {
    static constexpr unsigned kWidth = 16;

    ALWAYS_INLINE explicit SwissGroup(const SwissCtrl* ctrl)
    {
        m_ctrl = simde_vld1q_u8(reinterpret_cast<const uint8_t*>(ctrl));
    }

    // Return a bitmask of positions that match h2.
    ALWAYS_INLINE SwissBitMask match(uint8_t h2) const
    {
#if CPU(X86_64)
        auto raw = simde_uint8x16_to_m128i(m_ctrl);
        auto match = simde_mm_cmpeq_epi8(raw, simde_mm_set1_epi8(static_cast<char>(h2)));
        return SwissBitMask(static_cast<uint32_t>(simde_mm_movemask_epi8(match)));
#else
        auto match = simde_vceqq_u8(m_ctrl, simde_vmovq_n_u8(h2));
        // On ARM, each matching byte becomes 0xFF. Extract one bit per byte.
        // Use a shift-and-add pattern to pack 16 comparison results into a uint16_t.
        static constexpr uint8_t shifts[] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
        auto shiftVector = simde_vld1q_u8(shifts);
        auto masked = simde_vandq_u8(match, shiftVector);
        // Sum the low and high halves.
        auto low = simde_vget_low_u8(masked);
        auto high = simde_vget_high_u8(masked);
        uint8_t lowByte = simde_vaddv_u8(low);
        uint8_t highByte = simde_vaddv_u8(high);
        return SwissBitMask(static_cast<uint32_t>(lowByte) | (static_cast<uint32_t>(highByte) << 8));
#endif
    }

    ALWAYS_INLINE SwissBitMask matchEmpty() const
    {
#if CPU(X86_64)
        auto raw = simde_uint8x16_to_m128i(m_ctrl);
        auto match = simde_mm_cmpeq_epi8(raw, simde_mm_set1_epi8(static_cast<char>(SwissCtrl::kEmpty)));
        return SwissBitMask(static_cast<uint32_t>(simde_mm_movemask_epi8(match)));
#else
        auto match = simde_vceqq_u8(m_ctrl, simde_vmovq_n_u8(static_cast<uint8_t>(SwissCtrl::kEmpty)));
        static constexpr uint8_t shifts[] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
        auto shiftVector = simde_vld1q_u8(shifts);
        auto masked = simde_vandq_u8(match, shiftVector);
        auto low = simde_vget_low_u8(masked);
        auto high = simde_vget_high_u8(masked);
        uint8_t lowByte = simde_vaddv_u8(low);
        uint8_t highByte = simde_vaddv_u8(high);
        return SwissBitMask(static_cast<uint32_t>(lowByte) | (static_cast<uint32_t>(highByte) << 8));
#endif
    }

    // Match empty or deleted. Both have MSB set.
    ALWAYS_INLINE SwissBitMask matchEmptyOrDeleted() const
    {
#if CPU(X86_64)
        auto raw = simde_uint8x16_to_m128i(m_ctrl);
        // MSB set means negative as signed byte. Use movemask which extracts MSB of each byte.
        return SwissBitMask(static_cast<uint32_t>(simde_mm_movemask_epi8(raw)));
#else
        // Check MSB: if MSB is set (value >= 128 unsigned), it's empty or deleted.
        auto highBit = simde_vcgtq_u8(m_ctrl, simde_vmovq_n_u8(0x7F));
        static constexpr uint8_t shifts[] = { 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128 };
        auto shiftVector = simde_vld1q_u8(shifts);
        auto masked = simde_vandq_u8(highBit, shiftVector);
        auto low = simde_vget_low_u8(masked);
        auto high = simde_vget_high_u8(masked);
        uint8_t lowByte = simde_vaddv_u8(low);
        uint8_t highByte = simde_vaddv_u8(high);
        return SwissBitMask(static_cast<uint32_t>(lowByte) | (static_cast<uint32_t>(highByte) << 8));
#endif
    }

private:
    simde_uint8x16_t m_ctrl;
};

// --- Triangular probe sequence ---
class SwissProbeSeq {
public:
    ALWAYS_INLINE SwissProbeSeq(unsigned hash, unsigned mask)
        : m_mask(mask)
        , m_offset(hash & mask)
        , m_index(0)
    {
    }

    ALWAYS_INLINE unsigned offset() const { return m_offset; }

    ALWAYS_INLINE void next()
    {
        m_index += SwissGroup::kWidth;
        m_offset = (m_offset + m_index) & m_mask;
    }

private:
    unsigned m_mask;
    unsigned m_offset;
    unsigned m_index;
};

// --- Forward declarations ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class SwissHashTable;

// --- SwissHashTable Iterator ---
template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
class SwissHashTableConstIterator {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(SwissHashTableConstIterator);
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Value;
    using difference_type = ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

private:
    using ValueType = Value;
    using ReferenceType = const ValueType&;
    using PointerType = const ValueType*;
    using iterator = HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using const_iterator = SwissHashTableConstIterator;

    friend HashTableType;
    friend iterator;

    ALWAYS_INLINE void skipEmptyBuckets()
    {
        while (m_ctrl != m_endCtrl) {
            if (swissCtrlIsFull(*m_ctrl)) {
                if constexpr (KeyTraits::hasIsWeakNullValueFunction) {
                    if (isHashTraitsWeakNullValue<KeyTraits>(Extractor::extract(*m_slot))) {
                        ++m_ctrl;
                        ++m_slot;
                        continue;
                    }
                }
                return;
            }
            ++m_ctrl;
            ++m_slot;
        }
    }

    SwissHashTableConstIterator(const HashTableType* table, SwissCtrl* ctrl, ValueType* slot, SwissCtrl* endCtrl)
        : m_ctrl(ctrl)
        , m_slot(slot)
        , m_endCtrl(endCtrl)
    {
        addIterator(table, this);
        skipEmptyBuckets();
    }

    SwissHashTableConstIterator(const HashTableType* table, SwissCtrl* ctrl, ValueType* slot, SwissCtrl* endCtrl, HashItemKnownGoodTag)
        : m_ctrl(ctrl)
        , m_slot(slot)
        , m_endCtrl(endCtrl)
    {
        addIterator(table, this);
    }

public:
    SwissHashTableConstIterator()
        : m_ctrl(nullptr)
        , m_slot(nullptr)
        , m_endCtrl(nullptr)
    {
        addIterator(static_cast<const HashTableType*>(nullptr), this);
    }

#if CHECK_HASHTABLE_ITERATORS
    ~SwissHashTableConstIterator()
    {
        removeIterator(this);
    }

    SwissHashTableConstIterator(const const_iterator& other)
        : m_ctrl(other.m_ctrl)
        , m_slot(other.m_slot)
        , m_endCtrl(other.m_endCtrl)
    {
        addIterator(other.m_table, this);
    }

    const_iterator& operator=(const const_iterator& other)
    {
        m_ctrl = other.m_ctrl;
        m_slot = other.m_slot;
        m_endCtrl = other.m_endCtrl;

        removeIterator(this);
        addIterator(other.m_table, this);

        return *this;
    }
#endif

    PointerType get() const
    {
        checkValidity();
        return m_slot;
    }

    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    const_iterator& operator++()
    {
        checkValidity();
        ASSERT(m_ctrl != m_endCtrl);
        ++m_ctrl;
        ++m_slot;
        skipEmptyBuckets();
        return *this;
    }

    bool operator==(const const_iterator& other) const
    {
        checkValidity(other);
        return m_ctrl == other.m_ctrl;
    }

    bool operator==(const iterator& other) const
    {
        return *this == static_cast<const_iterator>(other);
    }

private:
    void checkValidity() const
    {
#if CHECK_HASHTABLE_ITERATORS
        ASSERT(m_table);
#endif
    }

#if CHECK_HASHTABLE_ITERATORS
    void checkValidity(const const_iterator& other) const
    {
        ASSERT(m_table);
        ASSERT_UNUSED(other, other.m_table);
        ASSERT(m_table == other.m_table);
    }
#else
    void checkValidity(const const_iterator&) const { }
#endif

    SwissCtrl* m_ctrl;
    ValueType* m_slot;
    SwissCtrl* m_endCtrl;

#if CHECK_HASHTABLE_ITERATORS
public:
    mutable const HashTableType* m_table;
    mutable const_iterator* m_next;
    mutable const_iterator* m_previous;
#endif
};

// The mutable iterator wraps the const iterator, same pattern as HashTable.
template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
class SwissHashTableIterator {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(SwissHashTableIterator);
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Value;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

private:
    using ValueType = Value;
    using ReferenceType = ValueType&;
    using PointerType = ValueType*;
    using const_iterator = SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using iterator = SwissHashTableIterator;

    friend HashTableType;

    SwissHashTableIterator(HashTableType* table, SwissCtrl* ctrl, ValueType* slot, SwissCtrl* endCtrl)
        : m_iterator(table, ctrl, slot, endCtrl)
    {
    }

    SwissHashTableIterator(HashTableType* table, SwissCtrl* ctrl, ValueType* slot, SwissCtrl* endCtrl, HashItemKnownGoodTag tag)
        : m_iterator(table, ctrl, slot, endCtrl, tag)
    {
    }

public:
    SwissHashTableIterator() = default;

    PointerType get() const { return const_cast<PointerType>(m_iterator.get()); }
    ReferenceType operator*() const { return *get(); }
    PointerType operator->() const { return get(); }

    iterator& operator++() { ++m_iterator; return *this; }

    bool operator==(const iterator& other) const { return m_iterator == other.m_iterator; }
    bool operator==(const const_iterator& other) const { return m_iterator == other; }

    operator const_iterator() const { return m_iterator; }

private:
    const_iterator m_iterator;
};

// --- addIterator/removeIterator/invalidateIterators for SwissHashTable iterators ---
#if CHECK_HASHTABLE_ITERATORS

template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
void addIterator(const HashTableType* table, SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>* it)
{
    it->m_table = table;
    it->m_previous = nullptr;

    if (!table) {
        it->m_next = nullptr;
    } else {
        Locker locker { *table->m_mutex };
        ASSERT(table->m_iterators != it);
        it->m_next = table->m_iterators;
        table->m_iterators = it;
        if (it->m_next) {
            ASSERT(!it->m_next->m_previous);
            it->m_next->m_previous = it;
        }
    }
}

template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
void removeIterator(SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>* it)
{
    if (!it->m_table) {
        ASSERT(!it->m_next);
        ASSERT(!it->m_previous);
    } else {
        Locker locker { *it->m_table->m_mutex };
        if (it->m_next) {
            ASSERT(it->m_next->m_previous == it);
            it->m_next->m_previous = it->m_previous;
        }
        if (it->m_previous) {
            ASSERT(it->m_table->m_iterators != it);
            ASSERT(it->m_previous->m_next == it);
            it->m_previous->m_next = it->m_next;
        } else {
            ASSERT(it->m_table->m_iterators == it);
            it->m_table->m_iterators = it->m_next;
        }
    }

    it->m_table = nullptr;
    it->m_next = nullptr;
    it->m_previous = nullptr;
}

#else

template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
inline void addIterator(const HashTableType*, SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*) { }

template<typename HashTableType, typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits>
inline void removeIterator(SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>*) { }

#endif

// ===========================================================================
// SwissHashTable
// ===========================================================================
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
class SwissHashTable {
public:
    using HashTableType = SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>;
    using iterator = SwissHashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using const_iterator = SwissHashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using ValueTraits = Traits;
    using KeyType = Key;
    using ValueType = Value;
    using TakeType = typename ValueTraits::TakeType;
    using IdentityTranslatorType = IdentityHashTranslator<ValueTraits, HashFunctions>;
    using AddResult = HashTableAddResult<iterator>;

    // 7/8 load factor — SwissTable characteristic.
    static constexpr unsigned maxLoadNumerator = 7;
    static constexpr unsigned maxLoadDenominator = 8;
    static constexpr unsigned minLoad = 6;

    SwissHashTable()
#if CHECK_HASHTABLE_ITERATORS
        : m_iterators(nullptr)
        , m_mutex(makeUnique<Lock>())
#endif
    {
    }

    ~SwissHashTable()
    {
        invalidateIterators(this);
        if (m_ctrl)
            deallocateTable(m_ctrl, m_slots, m_capacity);
#if CHECK_HASHTABLE_USE_AFTER_DESTRUCTION
        m_ctrl = reinterpret_cast<SwissCtrl*>(static_cast<uintptr_t>(0xbbadbeef));
#endif
    }

    SwissHashTable(const SwissHashTable&);
    void swap(SwissHashTable&);
    SwissHashTable& operator=(const SwissHashTable&);

    SwissHashTable(SwissHashTable&&);
    SwissHashTable& operator=(SwissHashTable&&);

    iterator begin() LIFETIME_BOUND
    {
        return isEmpty() ? end() : makeIterator(m_ctrl, m_slots);
    }

    iterator end() LIFETIME_BOUND
    {
        // Sentinel is at m_ctrl + m_capacity.
        return makeKnownGoodIterator(m_ctrl + m_capacity, m_slots + m_capacity);
    }

    const_iterator begin() const LIFETIME_BOUND
    {
        return isEmpty() ? end() : makeConstIterator(m_ctrl, m_slots);
    }

    const_iterator end() const LIFETIME_BOUND
    {
        return makeKnownGoodConstIterator(m_ctrl + m_capacity, m_slots + m_capacity);
    }

    iterator random() LIFETIME_BOUND
    {
        if (isEmpty())
            return end();

        while (true) {
            unsigned index = weakRandomNumber<unsigned>() & (m_capacity - 1);
            if (swissCtrlIsFull(m_ctrl[index]))
                return makeKnownGoodIterator(m_ctrl + index, m_slots + index);
        }
    }

    const_iterator random() const LIFETIME_BOUND { return static_cast<const_iterator>(const_cast<SwissHashTable*>(this)->random()); }

    unsigned size() const { return m_size; }
    unsigned capacity() const { return m_capacity; }
    size_t byteSize() const { return m_ctrl ? ctrlBytesCount(m_capacity) + m_capacity * sizeof(ValueType) : 0; }
    bool isEmpty() const { return !m_size; }
    ALWAYS_INLINE bool isNullStorage() const { return !m_ctrl; }

    void reserveInitialCapacity(unsigned keyCount)
    {
        ASSERT(!m_ctrl);
        ASSERT(!m_capacity);

        unsigned minimumTableSize = KeyTraits::minimumTableSize;
        unsigned newCapacity = std::max(minimumTableSize, computeBestCapacity(keyCount));

        allocateTable(newCapacity);
    }

    template<ShouldValidateKey shouldValidateKey> AddResult add(const ValueType& value) LIFETIME_BOUND { return add<IdentityTranslatorType, shouldValidateKey>(Extractor::extract(value), [&]() ALWAYS_INLINE_LAMBDA { return value; }); }
    template<ShouldValidateKey shouldValidateKey> AddResult add(ValueType&& value) LIFETIME_BOUND { return add<IdentityTranslatorType, shouldValidateKey>(Extractor::extract(value), [&]() ALWAYS_INLINE_LAMBDA { return WTF::move(value); }); }

    template<typename HashTranslator, ShouldValidateKey> AddResult add(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND;
    template<typename HashTranslator, ShouldValidateKey> AddResult addPassingHashCode(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND;

    template<ShouldValidateKey shouldValidateKey> iterator find(const KeyType& key) LIFETIME_BOUND { return find<IdentityTranslatorType, shouldValidateKey>(key); }
    template<ShouldValidateKey shouldValidateKey> const_iterator find(const KeyType& key) const LIFETIME_BOUND { return find<IdentityTranslatorType, shouldValidateKey>(key); }
    template<ShouldValidateKey shouldValidateKey> bool contains(const KeyType& key) const { return contains<IdentityTranslatorType, shouldValidateKey>(key); }

    template<typename HashTranslator, ShouldValidateKey, typename T> iterator find(const T&) LIFETIME_BOUND;
    template<typename HashTranslator, ShouldValidateKey, typename T> const_iterator find(const T&) const LIFETIME_BOUND;
    template<typename HashTranslator, ShouldValidateKey, typename T> bool contains(const T&) const;

    template<ShouldValidateKey> void remove(const KeyType&);
    void remove(iterator);
    void removeWithoutEntryConsistencyCheck(iterator);
    void removeWithoutEntryConsistencyCheck(const_iterator);
    bool removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto&);
    void clear();

    size_t computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction);
    bool isEmptyIgnoringNullReferences() const requires (KeyTraits::hasIsWeakNullValueFunction);
    void removeWeakNullEntries() const requires (KeyTraits::hasIsWeakNullValueFunction);

    template<size_t inlineCapacity>
    Vector<TakeType, inlineCapacity> takeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto&);

    static bool isEmptyBucket(const ValueType& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
    static bool isWeakNullBucket(const ValueType& value) { return isHashTraitsWeakNullValue<KeyTraits>(Extractor::extract(value)); }
    static bool isDeletedBucket(const ValueType& value) { return KeyTraits::isDeletedValue(Extractor::extract(value)); }
    static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value) || isDeletedBucket(value); }
    static bool isEmptyOrDeletedOrWeakNullBucket(const ValueType& value) { return isEmptyBucket(value) || isDeletedBucket(value) || isWeakNullBucket(value); }

    template<ShouldValidateKey shouldValidateKey> ValueType* lookup(const Key& key) { return lookup<IdentityTranslatorType, shouldValidateKey>(key); }
    template<typename HashTranslator, ShouldValidateKey, typename T> ValueType* lookup(const T&);
    template<typename HashTranslator, ShouldValidateKey, typename T> ValueType* inlineLookup(const T&);

#if ASSERT_ENABLED
    void checkTableConsistency() const;
#else
    static void checkTableConsistency() { }
#endif

#if CHECK_HASHTABLE_CONSISTENCY
    void internalCheckTableConsistency() const { checkTableConsistency(); }
    void internalCheckTableConsistencyExceptSize() const { checkTableConsistencyExceptSize(); }
#else
    static void internalCheckTableConsistencyExceptSize() { }
    static void internalCheckTableConsistency() { }
#endif

private:
    template<typename HashTranslator, ShouldValidateKey, typename T> void checkKey(const T&);

    void allocateTable(unsigned newCapacity);
    static void deallocateTable(SwissCtrl* ctrl, ValueType* slots, unsigned capacity);

    static constexpr unsigned ctrlBytesCount(unsigned capacity)
    {
        // capacity + 1 (sentinel) + kWidth - 1 (cloned bytes for wrap-around SIMD loads)
        return capacity + SwissGroup::kWidth;
    }

    static constexpr unsigned computeGrowthLeft(unsigned capacity)
    {
        return capacity * maxLoadNumerator / maxLoadDenominator - 0; // Capacity * 7/8
    }

    static constexpr unsigned computeBestCapacity(unsigned keyCount)
    {
        unsigned cap = roundUpToPowerOfTwo(std::max(keyCount, 1u));
        // Ensure we have room: keyCount must be <= cap * 7/8
        while (keyCount > cap * maxLoadNumerator / maxLoadDenominator)
            cap *= 2;
        return std::max(cap, KeyTraits::minimumTableSize);
    }

    static constexpr bool shouldExpand(unsigned size, unsigned capacity)
    {
        return size * maxLoadDenominator >= capacity * maxLoadNumerator;
    }

    bool shouldExpand() const { return m_growthLeft == 0 && m_size > 0; }
    bool shouldShrink() const { return m_size * minLoad < m_capacity && m_capacity > KeyTraits::minimumTableSize; }

    void expand();
    void shrink() { rehash(m_capacity / 2); }
    void shrinkToBestSize();
    void rehash(unsigned newCapacity);

    // Insert a value into a table that is known to have room. Used during rehash.
    void uncheckedInsert(ValueType&& value);

    // Find the first empty or deleted slot in the probe sequence starting at hash.
    unsigned findInsertSlot(unsigned hash) const;

    void removeInternal(SwissCtrl* ctrl, ValueType* slot);
    void removeAndInvalidateWithoutEntryConsistencyCheck(SwissCtrl* ctrl, ValueType* slot);
    void removeAndInvalidate(SwissCtrl* ctrl, ValueType* slot);

    void deleteWeakNullEntries();

    // Set a control byte and mirror it in the cloned range if necessary.
    ALWAYS_INLINE void setCtrl(unsigned index, SwissCtrl ctrl)
    {
        m_ctrl[index] = ctrl;
        // Mirror into the cloned range at the end: indices [0..kWidth-2] are also at [capacity..capacity+kWidth-2].
        if (index < SwissGroup::kWidth - 1)
            m_ctrl[m_capacity + index] = ctrl;
    }

    static void initializeBucket(ValueType& bucket)
    {
        initializeHashTableBucket<Traits>(bucket);
    }

    static void deleteBucket(ValueType& bucket)
    {
        hashTraitsDeleteBucket<Traits>(bucket);
    }

    iterator makeIterator(SwissCtrl* ctrl, ValueType* slot) { return iterator(this, ctrl, slot, m_ctrl + m_capacity); }
    const_iterator makeConstIterator(SwissCtrl* ctrl, ValueType* slot) const { return const_iterator(this, ctrl, slot, m_ctrl + m_capacity); }
    iterator makeKnownGoodIterator(SwissCtrl* ctrl, ValueType* slot) { return iterator(this, ctrl, slot, m_ctrl + m_capacity, HashItemKnownGood); }
    const_iterator makeKnownGoodConstIterator(SwissCtrl* ctrl, ValueType* slot) const { return const_iterator(this, ctrl, slot, m_ctrl + m_capacity, HashItemKnownGood); }

#if ASSERT_ENABLED
    void checkTableConsistencyExceptSize() const;
#else
    static void checkTableConsistencyExceptSize() { }
#endif

    SwissCtrl* m_ctrl { nullptr };
    ValueType* m_slots { nullptr };
    unsigned m_capacity { 0 };
    unsigned m_size { 0 };
    unsigned m_growthLeft { 0 };

#if CHECK_HASHTABLE_ITERATORS
public:
    mutable const_iterator* m_iterators;
    mutable std::unique_ptr<Lock> m_mutex;
#endif

#if DUMP_HASHTABLE_STATS_PER_TABLE
public:
    mutable std::unique_ptr<typename HashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::Stats> m_stats;
#endif
};

// ===========================================================================
// Implementation
// ===========================================================================

// --- checkKey ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::checkKey(const T& key)
{
    if constexpr (!ASSERT_ENABLED && shouldValidateKey == ShouldValidateKey::No)
        return;

    if (!HashFunctions::safeToCompareToEmptyOrDeleted)
        return;
    RELEASE_ASSERT(!HashTranslator::equal(KeyTraits::emptyValue(), key));
    AlignedStorage<ValueType> deletedValueBuffer;
    auto& deletedValue = *deletedValueBuffer;
    Traits::constructDeletedValue(deletedValue);
    RELEASE_ASSERT(!HashTranslator::equal(Extractor::extract(deletedValue), key));
}

// --- allocateTable ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::allocateTable(unsigned newCapacity)
{
    ASSERT(!(newCapacity & (newCapacity - 1))); // Must be power of 2.
    unsigned ctrlBytes = ctrlBytesCount(newCapacity);
    unsigned ctrlAligned = ctrlBytes;
    if (ctrlAligned % alignof(ValueType))
        ctrlAligned += alignof(ValueType) - (ctrlAligned % alignof(ValueType));

    size_t totalSize = ctrlAligned + static_cast<size_t>(newCapacity) * sizeof(ValueType);
    char* raw = static_cast<char*>(Malloc::zeroedMalloc(totalSize));

    m_ctrl = reinterpret_cast<SwissCtrl*>(raw);
    m_slots = reinterpret_cast<ValueType*>(raw + ctrlAligned);
    m_capacity = newCapacity;
    m_growthLeft = computeGrowthLeft(newCapacity);

    // Initialize control bytes to kEmpty (0x80). zeroedMalloc set them to 0, so we need to fill.
    memset(m_ctrl, static_cast<uint8_t>(SwissCtrl::kEmpty), ctrlBytes);
    // Set the sentinel.
    m_ctrl[newCapacity] = SwissCtrl::kSentinel;

    // Initialize slots to empty values if emptyValueIsZero is false.
    if constexpr (!Traits::emptyValueIsZero) {
        for (unsigned i = 0; i < newCapacity; i++)
            initializeBucket(m_slots[i]);
    }
}

// --- deallocateTable ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::deallocateTable(SwissCtrl* ctrl, ValueType* slots, unsigned capacity)
{
    for (unsigned i = 0; i < capacity; ++i) {
        if (swissCtrlIsFull(ctrl[i]))
            slots[i].~ValueType();
    }
    // ctrl is the start of the allocation.
    Malloc::free(ctrl);
}

// --- inlineLookup ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
ALWAYS_INLINE auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::inlineLookup(const T& key) -> ValueType*
{
    static_assert(sizeof(Value) <= 150, "Your HashTable types are too big to efficiently move when rehashing. Consider using UniqueRef instead");

    if (!m_ctrl)
        return nullptr;

    checkKey<HashTranslator, shouldValidateKey>(key);

    unsigned hash = HashTranslator::hash(key);
    uint8_t h2 = swissH2(hash);
    unsigned mask = m_capacity - 1;

    for (SwissProbeSeq seq(hash, mask); ; seq.next()) {
        SwissGroup group(m_ctrl + seq.offset());

        for (auto bitmask = group.match(h2); bitmask; ++bitmask) {
            unsigned index = (seq.offset() + bitmask.lowestSetBit()) & mask;
            ValueType* entry = m_slots + index;
            if (HashTranslator::equal(Extractor::extract(*entry), key))
                return entry;
        }

        if (group.matchEmpty())
            return nullptr;
    }
}

// --- lookup ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::lookup(const T& key) -> ValueType*
{
    return inlineLookup<HashTranslator, shouldValidateKey>(key);
}

// --- find ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::find(const T& key) LIFETIME_BOUND -> iterator
{
    if (!m_ctrl)
        return end();

    ValueType* entry = lookup<HashTranslator, shouldValidateKey>(key);
    if (!entry)
        return end();

    unsigned index = entry - m_slots;
    return makeKnownGoodIterator(m_ctrl + index, entry);
}

// --- find const ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::find(const T& key) const LIFETIME_BOUND -> const_iterator
{
    if (!m_ctrl)
        return end();

    ValueType* entry = const_cast<SwissHashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
    if (!entry)
        return end();

    unsigned index = entry - m_slots;
    return makeKnownGoodConstIterator(m_ctrl + index, entry);
}

// --- contains ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
bool SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::contains(const T& key) const
{
    if (!m_ctrl)
        return false;

    return const_cast<SwissHashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
}

// --- findInsertSlot ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
unsigned SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::findInsertSlot(unsigned hash) const
{
    unsigned mask = m_capacity - 1;
    for (SwissProbeSeq seq(hash, mask); ; seq.next()) {
        SwissGroup group(m_ctrl + seq.offset());
        auto bitmask = group.matchEmptyOrDeleted();
        if (bitmask)
            return (seq.offset() + bitmask.lowestSetBit()) & mask;
    }
}

// --- add ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey>
ALWAYS_INLINE auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::add(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
{
    checkKey<HashTranslator, shouldValidateKey>(key);
    invalidateIterators(this);

    if (!m_ctrl)
        allocateTable(KeyTraits::minimumTableSize);

    internalCheckTableConsistency();

    unsigned hash = HashTranslator::hash(key);
    uint8_t h2 = swissH2(hash);
    unsigned mask = m_capacity - 1;

    // First, search for the key.
    for (SwissProbeSeq seq(hash, mask); ; seq.next()) {
        SwissGroup group(m_ctrl + seq.offset());

        for (auto bitmask = group.match(h2); bitmask; ++bitmask) {
            unsigned index = (seq.offset() + bitmask.lowestSetBit()) & mask;
            ValueType* entry = m_slots + index;
            if (HashTranslator::equal(Extractor::extract(*entry), key))
                return AddResult(makeKnownGoodIterator(m_ctrl + index, entry), false);
        }

        if (group.matchEmpty())
            break;
    }

    // Key not found. Check growth.
    if (m_growthLeft == 0) {
        expand();
        // Hash is still valid; mask may have changed.
        mask = m_capacity - 1;
    }

    // Find an insert slot.
    unsigned insertIndex = findInsertSlot(hash);
    bool wasEmpty = swissCtrlIsEmpty(m_ctrl[insertIndex]);

    // Initialize the slot if we need to transition from the "deleted" marker.
    if (!wasEmpty) {
        // Slot was deleted; destroy old deleted-marker value and re-initialize.
        m_slots[insertIndex].~ValueType();
        initializeBucket(m_slots[insertIndex]);
    }

    HashTranslator::translate(m_slots[insertIndex], std::forward<decltype(key)>(key), functor);
    setCtrl(insertIndex, static_cast<SwissCtrl>(h2));
    m_size++;
    if (wasEmpty)
        m_growthLeft--;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(m_ctrl + insertIndex, m_slots + insertIndex), true);
}

// --- addPassingHashCode ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::addPassingHashCode(auto&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
{
    checkKey<HashTranslator, shouldValidateKey>(key);
    invalidateIterators(this);

    if (!m_ctrl)
        allocateTable(KeyTraits::minimumTableSize);

    internalCheckTableConsistency();

    unsigned hash = HashTranslator::hash(key);
    uint8_t h2 = swissH2(hash);
    unsigned mask = m_capacity - 1;

    // Search for the key.
    for (SwissProbeSeq seq(hash, mask); ; seq.next()) {
        SwissGroup group(m_ctrl + seq.offset());

        for (auto bitmask = group.match(h2); bitmask; ++bitmask) {
            unsigned index = (seq.offset() + bitmask.lowestSetBit()) & mask;
            ValueType* entry = m_slots + index;
            if (HashTranslator::equal(Extractor::extract(*entry), key))
                return AddResult(makeKnownGoodIterator(m_ctrl + index, entry), false);
        }

        if (group.matchEmpty())
            break;
    }

    if (m_growthLeft == 0) {
        expand();
        mask = m_capacity - 1;
    }

    unsigned insertIndex = findInsertSlot(hash);
    bool wasEmpty = swissCtrlIsEmpty(m_ctrl[insertIndex]);

    if (!wasEmpty) {
        m_slots[insertIndex].~ValueType();
        initializeBucket(m_slots[insertIndex]);
    }

    HashTranslator::translate(m_slots[insertIndex], std::forward<decltype(key)>(key), functor, hash);
    setCtrl(insertIndex, static_cast<SwissCtrl>(h2));
    m_size++;
    if (wasEmpty)
        m_growthLeft--;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(m_ctrl + insertIndex, m_slots + insertIndex), true);
}

// --- uncheckedInsert (for rehash) ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::uncheckedInsert(ValueType&& value)
{
    unsigned hash = IdentityTranslatorType::hash(Extractor::extract(value));
    uint8_t h2 = swissH2(hash);
    unsigned insertIndex = findInsertSlot(hash);

    m_slots[insertIndex].~ValueType();
    new (NotNull, m_slots + insertIndex) ValueType(WTF::move(value));
    setCtrl(insertIndex, static_cast<SwissCtrl>(h2));
}

// --- removeInternal ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeInternal(SwissCtrl* ctrl, ValueType* slot)
{
    unsigned index = ctrl - m_ctrl;

    slot->~ValueType();
    initializeBucket(*slot);

    // Optimization: if the group containing this slot has any empty slot, we can
    // mark this as empty (recovering growth budget), rather than deleted.
    SwissGroup group(m_ctrl + (index & ~(SwissGroup::kWidth - 1)));
    if (group.matchEmpty()) {
        setCtrl(index, SwissCtrl::kEmpty);
        m_growthLeft++;
    } else {
        setCtrl(index, SwissCtrl::kDeleted);
    }

    m_size--;

    if (shouldShrink())
        shrink();

    internalCheckTableConsistency();
}

// --- removeAndInvalidateWithoutEntryConsistencyCheck ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeAndInvalidateWithoutEntryConsistencyCheck(SwissCtrl* ctrl, ValueType* slot)
{
    invalidateIterators(this);
    removeInternal(ctrl, slot);
}

// --- removeAndInvalidate ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeAndInvalidate(SwissCtrl* ctrl, ValueType* slot)
{
    invalidateIterators(this);
    internalCheckTableConsistency();
    removeInternal(ctrl, slot);
}

// --- remove(iterator) ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::remove(iterator it)
{
    if (it == end())
        return;

    auto* slot = const_cast<ValueType*>(it.m_iterator.m_slot);
    auto* ctrl = const_cast<SwissCtrl*>(it.m_iterator.m_ctrl);
    removeAndInvalidate(ctrl, slot);
}

// --- removeWithoutEntryConsistencyCheck(iterator) ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWithoutEntryConsistencyCheck(iterator it)
{
    if (it == end())
        return;

    auto* slot = const_cast<ValueType*>(it.m_iterator.m_slot);
    auto* ctrl = const_cast<SwissCtrl*>(it.m_iterator.m_ctrl);
    removeAndInvalidateWithoutEntryConsistencyCheck(ctrl, slot);
}

// --- removeWithoutEntryConsistencyCheck(const_iterator) ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWithoutEntryConsistencyCheck(const_iterator it)
{
    if (it == end())
        return;

    auto* slot = const_cast<ValueType*>(it.m_slot);
    auto* ctrl = const_cast<SwissCtrl*>(it.m_ctrl);
    removeAndInvalidateWithoutEntryConsistencyCheck(ctrl, slot);
}

// --- remove(key) ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<ShouldValidateKey shouldValidateKey>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::remove(const KeyType& key)
{
    remove(find<shouldValidateKey>(key));
}

// --- removeIf ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline bool SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto& functor)
{
    unsigned removedCount = 0;

    for (unsigned i = 0; i < m_capacity; i++) {
        if (!swissCtrlIsFull(m_ctrl[i]))
            continue;

        ValueType& slot = m_slots[i];

        if (isWeakNullBucket(slot)) {
            // Remove weak-null entries unconditionally.
        } else if (!functor(slot))
            continue;

        slot.~ValueType();
        initializeBucket(slot);

        SwissGroup group(m_ctrl + (i & ~(SwissGroup::kWidth - 1)));
        if (group.matchEmpty()) {
            setCtrl(i, SwissCtrl::kEmpty);
            m_growthLeft++;
        } else {
            setCtrl(i, SwissCtrl::kDeleted);
        }

        removedCount++;
    }

    m_size -= removedCount;

    if (shouldShrink())
        shrinkToBestSize();

    internalCheckTableConsistency();
    return removedCount;
}

// --- takeIf ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
template<size_t inlineCapacity>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::takeIf(NOESCAPE const Invocable<bool(const ValueType&)> auto& functor) -> Vector<TakeType, inlineCapacity>
{
    Vector<TakeType, inlineCapacity> result;

    removeIf([&](ValueType& value) {
        if (!functor(value))
            return false;

        result.append(ValueTraits::take(WTF::move(value)));
        return true;
    });

    return result;
}

// --- WeakPtr support ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline size_t SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::computeSize() const requires (KeyTraits::hasIsWeakNullValueFunction)
{
    removeWeakNullEntries();
    return size();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline bool SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::isEmptyIgnoringNullReferences() const requires (KeyTraits::hasIsWeakNullValueFunction)
{
    return isEmpty() || begin() == end();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::removeWeakNullEntries() const requires (KeyTraits::hasIsWeakNullValueFunction)
{
    const_cast<SwissHashTable&>(*this).removeIf([](ValueType&) {
        return false;
    });
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::deleteWeakNullEntries()
{
    unsigned removedCount = 0;

    for (unsigned i = 0; i < m_capacity; i++) {
        if (!swissCtrlIsFull(m_ctrl[i]))
            continue;

        if (!isWeakNullBucket(m_slots[i]))
            continue;

        m_slots[i].~ValueType();
        initializeBucket(m_slots[i]);

        SwissGroup group(m_ctrl + (i & ~(SwissGroup::kWidth - 1)));
        if (group.matchEmpty()) {
            setCtrl(i, SwissCtrl::kEmpty);
            m_growthLeft++;
        } else {
            setCtrl(i, SwissCtrl::kDeleted);
        }

        removedCount++;
    }

    m_size -= removedCount;
    internalCheckTableConsistency();
}

// --- expand ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::expand()
{
    if constexpr (KeyTraits::hasIsWeakNullValueFunction)
        deleteWeakNullEntries();

    unsigned newCapacity;
    if (!m_capacity)
        newCapacity = KeyTraits::minimumTableSize;
    else
        newCapacity = m_capacity * 2;

    rehash(newCapacity);
}

// --- shrinkToBestSize ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::shrinkToBestSize()
{
    unsigned minimumTableSize = KeyTraits::minimumTableSize;
    rehash(std::max(minimumTableSize, computeBestCapacity(m_size)));
}

// --- rehash ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::rehash(unsigned newCapacity)
{
    internalCheckTableConsistencyExceptSize();

    SwissCtrl* oldCtrl = m_ctrl;
    ValueType* oldSlots = m_slots;
    unsigned oldCapacity = m_capacity;
    unsigned oldSize = m_size;

    allocateTable(newCapacity);
    m_size = 0;

    for (unsigned i = 0; i < oldCapacity; ++i) {
        if (!swissCtrlIsFull(oldCtrl[i]))
            continue;

        if constexpr (KeyTraits::hasIsWeakNullValueFunction) {
            if (isWeakNullBucket(oldSlots[i])) {
                oldSlots[i].~ValueType();
                oldSize--;
                continue;
            }
        }

        uncheckedInsert(WTF::move(oldSlots[i]));
        oldSlots[i].~ValueType();
        m_size++;
    }

    ASSERT_UNUSED(oldSize, m_size == oldSize || KeyTraits::hasIsWeakNullValueFunction);

    if (oldCtrl)
        Malloc::free(oldCtrl);

    internalCheckTableConsistency();
}

// --- clear ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::clear()
{
    invalidateIterators(this);
    if (!m_ctrl)
        return;

    SwissCtrl* oldCtrl = m_ctrl;
    ValueType* oldSlots = m_slots;
    unsigned oldCapacity = m_capacity;

    m_ctrl = nullptr;
    m_slots = nullptr;
    m_capacity = 0;
    m_size = 0;
    m_growthLeft = 0;

    deallocateTable(oldCtrl, oldSlots, oldCapacity);
    internalCheckTableConsistency();
}

// --- copy constructor ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::SwissHashTable(const SwissHashTable& other)
#if CHECK_HASHTABLE_ITERATORS
    : m_iterators(nullptr)
    , m_mutex(makeUnique<Lock>())
#endif
{
    if (!other.m_capacity || !other.m_size)
        return;

    unsigned newCapacity = computeBestCapacity(other.m_size);
    allocateTable(newCapacity);

    for (unsigned i = 0; i < other.m_capacity; ++i) {
        if (!swissCtrlIsFull(other.m_ctrl[i]))
            continue;

        if constexpr (KeyTraits::hasIsWeakNullValueFunction) {
            if (isWeakNullBucket(other.m_slots[i]))
                continue;
        }

        ValueType entry(other.m_slots[i]);
        uncheckedInsert(WTF::move(entry));
        m_size++;
    }

    internalCheckTableConsistency();
}

// --- swap ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::swap(SwissHashTable& other)
{
    using std::swap;
    invalidateIterators(this);
    invalidateIterators(&other);

    swap(m_ctrl, other.m_ctrl);
    swap(m_slots, other.m_slots);
    swap(m_capacity, other.m_capacity);
    swap(m_size, other.m_size);
    swap(m_growthLeft, other.m_growthLeft);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

// --- copy assignment ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::operator=(const SwissHashTable& other) -> SwissHashTable&
{
    if (&other == this)
        return *this;

    SwissHashTable tmp(other);
    swap(tmp);
    return *this;
}

// --- move constructor ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::SwissHashTable(SwissHashTable&& other)
#if CHECK_HASHTABLE_ITERATORS
    : m_iterators(nullptr)
    , m_mutex(makeUnique<Lock>())
#endif
{
    invalidateIterators(&other);

    m_ctrl = std::exchange(other.m_ctrl, nullptr);
    m_slots = std::exchange(other.m_slots, nullptr);
    m_capacity = std::exchange(other.m_capacity, 0);
    m_size = std::exchange(other.m_size, 0);
    m_growthLeft = std::exchange(other.m_growthLeft, 0);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

// --- move assignment ---
template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::operator=(SwissHashTable&& other) -> SwissHashTable&
{
    SwissHashTable temp(WTF::move(other));
    swap(temp);
    return *this;
}

// --- consistency checks ---
#if ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::checkTableConsistency() const
{
    checkTableConsistencyExceptSize();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>::checkTableConsistencyExceptSize() const
{
    if (!m_ctrl)
        return;

    unsigned count = 0;
    for (unsigned i = 0; i < m_capacity; ++i) {
        if (!swissCtrlIsFull(m_ctrl[i]))
            continue;

        if constexpr (KeyTraits::hasIsWeakNullValueFunction) {
            if (isWeakNullBucket(m_slots[i])) {
                ++count;
                continue;
            }
        }

        auto& key = Extractor::extract(m_slots[i]);
        const_iterator it = find<ShouldValidateKey::No>(key);
        ASSERT(m_slots + i == it.m_slot);
        ++count;

        ValueCheck<Key>::checkConsistency(key);
    }

    ASSERT(count == m_size);
    ASSERT(m_capacity >= KeyTraits::minimumTableSize);
    ASSERT(!(m_capacity & (m_capacity - 1))); // Power of 2.
}

#endif // ASSERT_ENABLED

// ===========================================================================
// SwissHashTableTraits
// ===========================================================================
struct SwissHashTableTraits {
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    using TableType = SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, Malloc>;
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
