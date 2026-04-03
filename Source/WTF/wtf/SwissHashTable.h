/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

// SwissTable (flat hash map) implementation for WTF.
// Uses SIMD-accelerated control byte matching (NEON on ARM64, SSE2 on x86-64)
// with triangular probing and 87.5% load factor.
// Unlike RobinHoodHashTable, does not require hasHashInValue.

#pragma once

#include <bit>
#include <cstring>
#include <numeric>
#include <wtf/AlignedStorage.h>
#include <wtf/HashTable.h>
#include <wtf/text/StringHash.h>

#if CPU(ARM64)
#include <arm_neon.h>
#elif CPU(X86_64)
#include <emmintrin.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WTF {

// --- Control byte constants ---

using SwissCtrl = int8_t;

static constexpr SwissCtrl kSwissEmpty = static_cast<SwissCtrl>(0x80);     // -128
static constexpr SwissCtrl kSwissDeleted = static_cast<SwissCtrl>(0xFE);   // -2
static constexpr SwissCtrl kSwissSentinel = static_cast<SwissCtrl>(0xFF);  // -1
// H2 values are 0x00 - 0x7F (non-negative).

// --- BitMask ---

template<typename T, unsigned BitsPerLane>
class SwissBitMask {
    T m_mask;
public:
    explicit SwissBitMask(T mask) : m_mask(mask) { }
    explicit operator bool() const { return m_mask != 0; }

    unsigned operator*() const
    {
        ASSERT(m_mask);
        if constexpr (BitsPerLane == 1)
            return std::countr_zero(m_mask);
        else
            return std::countr_zero(m_mask) / BitsPerLane;
    }

    SwissBitMask& operator++()
    {
        if constexpr (BitsPerLane == 1) {
            m_mask &= m_mask - 1;
        } else {
            unsigned lane = **this;
            T laneMask = static_cast<T>(((T{1} << BitsPerLane) - 1)) << (lane * BitsPerLane);
            m_mask &= ~laneMask;
        }
        return *this;
    }
};

// --- SIMD Group Implementations ---

#if CPU(ARM64)

class SwissGroupNeon {
public:
    static constexpr unsigned kWidth = 16;
    using BitMaskType = SwissBitMask<uint64_t, 4>;

    SwissGroupNeon(const SwissCtrl* pos)
        : m_ctrl(vld1q_u8(reinterpret_cast<const uint8_t*>(pos)))
    {
    }

    BitMaskType match(SwissCtrl h2) const
    {
        uint8x16_t cmp = vceqq_u8(m_ctrl, vdupq_n_u8(static_cast<uint8_t>(h2)));
        return BitMaskType(compressMask(cmp));
    }

    BitMaskType matchEmpty() const
    {
        uint8x16_t cmp = vceqq_u8(m_ctrl, vdupq_n_u8(static_cast<uint8_t>(kSwissEmpty)));
        return BitMaskType(compressMask(cmp));
    }

    BitMaskType matchEmptyOrDeleted() const
    {
        // kEmpty (-128) and kDeleted (-2) are both < kSentinel (-1) in signed comparison.
        // h2 values (0-127) and kSentinel (-1) are NOT less than kSentinel.
        int8x16_t sentinel = vdupq_n_s8(kSwissSentinel);
        uint8x16_t result = vreinterpretq_u8_s8(
            vcgtq_s8(sentinel, vreinterpretq_s8_u8(m_ctrl)));
        return BitMaskType(compressMask(result));
    }

private:
    static uint64_t compressMask(uint8x16_t v)
    {
        // Compress 16 bytes (0xFF or 0x00) into 64 bits with 4 bits per lane.
        uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(v), 4);
        return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
    }

    uint8x16_t m_ctrl;
};

using SwissGroup = SwissGroupNeon;

#elif CPU(X86_64)

class SwissGroupSSE2 {
public:
    static constexpr unsigned kWidth = 16;
    using BitMaskType = SwissBitMask<uint32_t, 1>;

    SwissGroupSSE2(const SwissCtrl* pos)
        : m_ctrl(_mm_loadu_si128(reinterpret_cast<const __m128i*>(pos)))
    {
    }

    BitMaskType match(SwissCtrl h2) const
    {
        return BitMaskType(
            _mm_movemask_epi8(_mm_cmpeq_epi8(m_ctrl, _mm_set1_epi8(h2))));
    }

    BitMaskType matchEmpty() const
    {
        return match(kSwissEmpty);
    }

    BitMaskType matchEmptyOrDeleted() const
    {
        // _mm_cmpgt_epi8(a, b) returns 0xFF where a > b (signed).
        // kSentinel (-1) > kEmpty (-128) and kSentinel (-1) > kDeleted (-2), true.
        // kSentinel (-1) > kSentinel (-1), false.
        // kSentinel (-1) > h2 (0-127), false.
        auto sentinel = _mm_set1_epi8(kSwissSentinel);
        return BitMaskType(
            static_cast<uint32_t>(_mm_movemask_epi8(_mm_cmpgt_epi8(sentinel, m_ctrl))));
    }

private:
    __m128i m_ctrl;
};

using SwissGroup = SwissGroupSSE2;

#else

class SwissGroupPortable {
public:
    static constexpr unsigned kWidth = 8;
    using BitMaskType = SwissBitMask<uint32_t, 1>;

    SwissGroupPortable(const SwissCtrl* pos)
    {
        memcpy(m_ctrl, pos, kWidth);
    }

    BitMaskType match(SwissCtrl h2) const
    {
        uint32_t mask = 0;
        for (unsigned i = 0; i < kWidth; ++i) {
            if (m_ctrl[i] == h2)
                mask |= (1u << i);
        }
        return BitMaskType(mask);
    }

    BitMaskType matchEmpty() const
    {
        uint32_t mask = 0;
        for (unsigned i = 0; i < kWidth; ++i) {
            if (m_ctrl[i] == kSwissEmpty)
                mask |= (1u << i);
        }
        return BitMaskType(mask);
    }

    BitMaskType matchEmptyOrDeleted() const
    {
        uint32_t mask = 0;
        for (unsigned i = 0; i < kWidth; ++i) {
            // Signed comparison: kEmpty (-128) and kDeleted (-2) are < kSentinel (-1)
            if (m_ctrl[i] < kSwissSentinel)
                mask |= (1u << i);
        }
        return BitMaskType(mask);
    }

private:
    SwissCtrl m_ctrl[kWidth];
};

using SwissGroup = SwissGroupPortable;

#endif // CPU selection

// --- Size Policy ---

struct SwissHashTableSizePolicy {
    static constexpr unsigned maxLoadNumerator = 7;
    static constexpr unsigned maxLoadDenominator = 8; // 87.5%
    static constexpr unsigned minLoad = 6;
};

// --- Forward declaration ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions,
         typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
class SwissHashTable;

// --- SwissHashTable ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions,
         typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
class SwissHashTable {
public:
    using HashTableType = SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>;
    using iterator = HashTableIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using const_iterator = HashTableConstIterator<HashTableType, Key, Value, Extractor, HashFunctions, Traits, KeyTraits>;
    using ValueTraits = Traits;
    using KeyType = Key;
    using ValueType = Value;
    using IdentityTranslatorType = IdentityHashTranslator<ValueTraits, HashFunctions>;
    using AddResult = HashTableAddResult<iterator>;

    static_assert(!KeyTraits::hasIsWeakNullValueFunction);

    SwissHashTable() = default;

    ~SwissHashTable()
    {
        invalidateIterators(this);
        if (m_ctrl)
            deallocateTable(m_ctrl, m_slots, m_capacity);
    }

    SwissHashTable(const SwissHashTable&);
    void swap(SwissHashTable&);
    SwissHashTable& operator=(const SwissHashTable&);

    SwissHashTable(SwissHashTable&&);
    SwissHashTable& operator=(SwissHashTable&&);

    iterator begin() LIFETIME_BOUND { return isEmpty() ? end() : makeIterator(m_slots); }
    iterator end() LIFETIME_BOUND { return makeKnownGoodIterator(m_slots + m_capacity); }
    const_iterator begin() const LIFETIME_BOUND { return isEmpty() ? end() : makeConstIterator(m_slots); }
    const_iterator end() const LIFETIME_BOUND { return makeKnownGoodConstIterator(m_slots + m_capacity); }

    iterator random() LIFETIME_BOUND
    {
        if (isEmpty())
            return end();

        while (true) {
            unsigned index = weakRandomNumber<uint32_t>() & m_capacity;
            if (index < m_capacity && m_ctrl[index] >= 0)
                return makeKnownGoodIterator(m_slots + index);
        }
    }

    const_iterator random() const LIFETIME_BOUND { return static_cast<const_iterator>(const_cast<SwissHashTable*>(this)->random()); }

    unsigned size() const { return m_keyCount; }
    unsigned capacity() const { return m_capacity; }
    bool isEmpty() const { return !m_keyCount; }
    ALWAYS_INLINE bool isNullStorage() const { return !m_ctrl; }

    void reserveInitialCapacity(unsigned keyCount)
    {
        ASSERT(!m_ctrl);
        ASSERT(!m_capacity);

        unsigned minimumTableSize = KeyTraits::minimumTableSize;
        unsigned newCapacity = computeBestTableSize(std::max(keyCount, minimumTableSize));

        auto [ctrl, slots] = allocateTable(newCapacity);
        m_ctrl = ctrl;
        m_slots = slots;
        m_capacity = newCapacity;
        m_keyCount = 0;
        m_deletedCount = 0;
        internalCheckTableConsistency();
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

    void remove(const KeyType&);
    void remove(iterator);
    void removeWithoutEntryConsistencyCheck(iterator);
    void removeWithoutEntryConsistencyCheck(const_iterator);
    bool removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto&);
    void clear();

    static bool isEmptyBucket(const ValueType& value) { return isHashTraitsEmptyValue<KeyTraits>(Extractor::extract(value)); }
    static bool isEmptyOrDeletedBucket(const ValueType& value) { return isEmptyBucket(value); }
    static bool isEmptyOrDeletedOrWeakNullBucket(const ValueType& value) { static_assert(!KeyTraits::hasIsWeakNullValueFunction); return isEmptyOrDeletedBucket(value); }

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

    static constexpr bool shouldExpand(uint64_t keyAndDeleteCount, uint64_t tableCapacity)
    {
        return keyAndDeleteCount * maxLoadDenominator >= tableCapacity * maxLoadNumerator;
    }

private:
    // --- H1/H2 hash splitting ---
    // We extract [17, 24] bits since StringImpl only offers 24 bits.
    static constexpr SwissCtrl h2(unsigned hash) { return static_cast<SwissCtrl>(hash >> 17) & 0x7F; }

    // --- Probe sequence ---
    struct ProbeSequence {
        unsigned m_offset;
        unsigned m_stride { 0 };
        unsigned m_mask;

        ProbeSequence(unsigned hash, unsigned mask)
            : m_offset(hash & mask), m_mask(mask) { }

        unsigned offset() const { return m_offset; }
        unsigned offset(unsigned i) const { return (m_offset + i) & m_mask; }

        void next()
        {
            m_stride += SwissGroup::kWidth;
            m_offset = (m_offset + m_stride) & m_mask;
        }
    };

    // --- Ctrl byte management ---
    void setCtrl(unsigned index, SwissCtrl value)
    {
        m_ctrl[index] = value;
        if (index < static_cast<unsigned>(SwissGroup::kWidth) - 1)
            m_ctrl[m_capacity + 1 + index] = value;
    }

    // --- Capacity helpers ---
    static constexpr unsigned numCtrlBytes(unsigned capacity) { return capacity + SwissGroup::kWidth; }

    static constexpr unsigned slotsOffset(unsigned capacity)
    {
        unsigned ctrlBytes = numCtrlBytes(capacity);
        return (ctrlBytes + alignof(ValueType) - 1) & ~(alignof(ValueType) - 1);
    }

    static constexpr unsigned capacityForSlots(unsigned n)
    {
        if (n == 0) return 0;
        unsigned minCap = std::max(n, static_cast<unsigned>(SwissGroup::kWidth) - 1);
        return roundUpToPowerOfTwo(minCap + 1) - 1;
    }

    static constexpr unsigned computeBestTableSize(unsigned keyCount);

    // --- Allocation ---
    static std::pair<SwissCtrl*, ValueType*> allocateTable(unsigned capacity);
    static void deallocateTable(SwissCtrl* ctrl, ValueType* slots, unsigned capacity);
    static void initializeBucket(ValueType& bucket) { initializeHashTableBucket<Traits>(bucket); }
    static void deleteBucket(ValueType& bucket) { hashTraitsDeleteBucket<Traits>(bucket); }

    // --- Internal operations ---
    unsigned findSlotForInsertion(unsigned hash) const;
    void reinsert(ValueType&&);
    void rehash(unsigned newCapacity);
    void expand();
    void shrink() { rehash(m_capacity >> 1); }
    void shrinkToBestSize();

    bool shouldExpand() const
    {
        return shouldExpand(static_cast<uint64_t>(m_keyCount) + m_deletedCount, m_capacity);
    }

    bool shouldShrink() const
    {
        unsigned minCap = capacityForSlots(KeyTraits::minimumTableSize);
        return m_keyCount * minLoad < m_capacity && m_capacity > minCap;
    }

    void removeAndInvalidateWithoutEntryConsistencyCheck(ValueType*);
    void removeAndInvalidate(ValueType*);
    void remove(ValueType*);

    template<typename HashTranslator, ShouldValidateKey, typename T> void checkKey(const T&);

    // --- Iterator helpers ---
    iterator makeIterator(ValueType* pos) { return iterator(this, pos, m_slots + m_capacity); }
    const_iterator makeConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_slots + m_capacity); }
    iterator makeKnownGoodIterator(ValueType* pos) { return iterator(this, pos, m_slots + m_capacity, HashItemKnownGood); }
    const_iterator makeKnownGoodConstIterator(ValueType* pos) const { return const_iterator(this, pos, m_slots + m_capacity, HashItemKnownGood); }

    // --- Load factor ---
    static constexpr unsigned maxLoadNumerator = SizePolicy::maxLoadNumerator;
    static constexpr unsigned maxLoadDenominator = SizePolicy::maxLoadDenominator;
    static constexpr unsigned minLoad = SizePolicy::minLoad;

    // --- Members ---
    SwissCtrl* m_ctrl { nullptr };
    ValueType* m_slots { nullptr };
    unsigned m_capacity { 0 };
    unsigned m_keyCount { 0 };
    unsigned m_deletedCount { 0 };

#if CHECK_HASHTABLE_ITERATORS
public:
    mutable const_iterator* m_iterators { nullptr };
    mutable std::unique_ptr<Lock> m_mutex { makeUnique<Lock>() };
#endif

#if ASSERT_ENABLED
    void checkTableConsistencyExceptSize() const;
#else
    static void checkTableConsistencyExceptSize() { }
#endif
};

// ==================== Implementation ====================

// --- checkKey ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::checkKey(const T& key)
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

// --- inlineLookup ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
ALWAYS_INLINE auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::inlineLookup(const T& key) -> ValueType*
{
    checkKey<HashTranslator, shouldValidateKey>(key);

    if (!m_ctrl)
        return nullptr;

    unsigned hash = HashTranslator::hash(key);
    SwissCtrl h2v = h2(hash);

    ProbeSequence seq(hash, m_capacity);
    while (true) {
        SwissGroup group(m_ctrl + seq.offset());
        auto match = group.match(h2v);
        while (match) {
            unsigned pos = seq.offset(*match);
            if (HashTranslator::equal(Extractor::extract(m_slots[pos]), key))
                return m_slots + pos;
            ++match;
        }
        if (group.matchEmpty())
            return nullptr;
        seq.next();
    }
}

// --- lookup ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::lookup(const T& key) -> ValueType*
{
    return inlineLookup<HashTranslator, shouldValidateKey>(key);
}

// --- find ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::find(const T& key) LIFETIME_BOUND -> iterator
{
    if (!m_ctrl)
        return end();

    ValueType* entry = lookup<HashTranslator, shouldValidateKey>(key);
    if (!entry)
        return end();

    return makeKnownGoodIterator(entry);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::find(const T& key) const LIFETIME_BOUND -> const_iterator
{
    if (!m_ctrl)
        return end();

    ValueType* entry = const_cast<SwissHashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
    if (!entry)
        return end();

    return makeKnownGoodConstIterator(entry);
}

// --- contains ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
bool SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::contains(const T& key) const
{
    if (!m_ctrl)
        return false;

    return const_cast<SwissHashTable*>(this)->lookup<HashTranslator, shouldValidateKey>(key);
}

// --- findSlotForInsertion ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
unsigned SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::findSlotForInsertion(unsigned hash) const
{
    ProbeSequence seq(hash, m_capacity);
    while (true) {
        SwissGroup group(m_ctrl + seq.offset());
        auto match = group.matchEmptyOrDeleted();
        if (match)
            return seq.offset(*match);
        seq.next();
    }
}

// --- add ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
ALWAYS_INLINE auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::add(T&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
{
    checkKey<HashTranslator, shouldValidateKey>(key);

    invalidateIterators(this);

    if (shouldExpand())
        expand();

    internalCheckTableConsistency();

    ASSERT(m_ctrl);

    unsigned hash = HashTranslator::hash(key);
    SwissCtrl h2v = h2(hash);

    // Search for existing entry.
    ProbeSequence seq(hash, m_capacity);
    while (true) {
        SwissGroup group(m_ctrl + seq.offset());
        auto match = group.match(h2v);
        while (match) {
            unsigned pos = seq.offset(*match);
            if (HashTranslator::equal(Extractor::extract(m_slots[pos]), key))
                return AddResult(makeKnownGoodIterator(m_slots + pos), false);
            ++match;
        }
        if (group.matchEmpty())
            break;
        seq.next();
    }

    // Not found. Insert.
    unsigned insertPos = findSlotForInsertion(hash);
    bool wasDeleted = m_ctrl[insertPos] == kSwissDeleted;

    HashTranslator::translate(m_slots[insertPos], std::forward<T>(key), functor);
    setCtrl(insertPos, h2v);
    m_keyCount++;
    if (wasDeleted)
        m_deletedCount--;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(m_slots + insertPos), true);
}

// --- addPassingHashCode ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
template<typename HashTranslator, ShouldValidateKey shouldValidateKey, typename T>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::addPassingHashCode(T&& key, NOESCAPE const std::invocable<> auto& functor) LIFETIME_BOUND -> AddResult
{
    checkKey<HashTranslator, shouldValidateKey>(key);

    invalidateIterators(this);

    if (shouldExpand())
        expand();

    internalCheckTableConsistency();

    ASSERT(m_ctrl);

    unsigned originalHash = HashTranslator::hash(key);
    unsigned hash = originalHash;
    SwissCtrl h2v = h2(hash);

    ProbeSequence seq(hash, m_capacity);
    while (true) {
        SwissGroup group(m_ctrl + seq.offset());
        auto match = group.match(h2v);
        while (match) {
            unsigned pos = seq.offset(*match);
            if (HashTranslator::equal(Extractor::extract(m_slots[pos]), key))
                return AddResult(makeKnownGoodIterator(m_slots + pos), false);
            ++match;
        }
        if (group.matchEmpty())
            break;
        seq.next();
    }

    unsigned insertPos = findSlotForInsertion(hash);
    bool wasDeleted = m_ctrl[insertPos] == kSwissDeleted;

    HashTranslator::translate(m_slots[insertPos], std::forward<T>(key), functor, originalHash);
    setCtrl(insertPos, h2v);
    m_keyCount++;
    if (wasDeleted)
        m_deletedCount--;

    internalCheckTableConsistency();

    return AddResult(makeKnownGoodIterator(m_slots + insertPos), true);
}

// --- remove ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::removeAndInvalidateWithoutEntryConsistencyCheck(ValueType* pos)
{
    invalidateIterators(this);
    remove(pos);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::removeAndInvalidate(ValueType* pos)
{
    invalidateIterators(this);
    internalCheckTableConsistency();
    remove(pos);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::remove(ValueType* pos)
{
    unsigned index = pos - m_slots;
    ASSERT(index < m_capacity);
    ASSERT(m_ctrl[index] >= 0);

    deleteBucket(*pos);
    initializeBucket(*pos);
    setCtrl(index, kSwissDeleted);

    m_keyCount--;
    m_deletedCount++;

    if (shouldShrink())
        shrinkToBestSize();

    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::remove(iterator it)
{
    if (it == end())
        return;

    removeAndInvalidate(const_cast<ValueType*>(it.m_iterator.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::removeWithoutEntryConsistencyCheck(iterator it)
{
    if (it == end())
        return;

    removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.m_iterator.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::removeWithoutEntryConsistencyCheck(const_iterator it)
{
    if (it == end())
        return;

    removeAndInvalidateWithoutEntryConsistencyCheck(const_cast<ValueType*>(it.m_position));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::remove(const KeyType& key)
{
    remove(find(key));
}

// --- removeIf ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline bool SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::removeIf(NOESCAPE const Invocable<bool(ValueType&)> auto& functor)
{
    if (!m_ctrl)
        return false;

    invalidateIterators(this);

    unsigned removedCount = 0;
    for (unsigned i = 0; i < m_capacity; ++i) {
        if (m_ctrl[i] < 0) // empty or deleted
            continue;

        if (!functor(m_slots[i]))
            continue;

        deleteBucket(m_slots[i]);
        initializeBucket(m_slots[i]);
        setCtrl(i, kSwissDeleted);
        ++removedCount;
    }
    if (removedCount) {
        m_keyCount -= removedCount;
        m_deletedCount += removedCount;
    }

    if (shouldShrink())
        shrinkToBestSize();

    internalCheckTableConsistency();
    return removedCount > 0;
}

// --- clear ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::clear()
{
    invalidateIterators(this);
    if (!m_ctrl)
        return;

    unsigned oldCapacity = m_capacity;
    SwissCtrl* oldCtrl = std::exchange(m_ctrl, nullptr);
    ValueType* oldSlots = std::exchange(m_slots, nullptr);
    m_capacity = 0;
    m_keyCount = 0;
    m_deletedCount = 0;
    deallocateTable(oldCtrl, oldSlots, oldCapacity);
    internalCheckTableConsistency();
}

// --- allocateTable / deallocateTable ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::allocateTable(unsigned capacity) -> std::pair<SwissCtrl*, ValueType*>
{
    unsigned offset = slotsOffset(capacity);
    size_t totalBytes = offset + static_cast<size_t>(capacity) * sizeof(ValueType);

    char* alloc;
    if constexpr (Traits::emptyValueIsZero)
        alloc = static_cast<char*>(HashTableMalloc::zeroedMalloc(totalBytes));
    else
        alloc = static_cast<char*>(HashTableMalloc::malloc(totalBytes));

    SwissCtrl* ctrl = reinterpret_cast<SwissCtrl*>(alloc);
    ValueType* slots = reinterpret_cast<ValueType*>(alloc + offset);

    // Initialize ctrl bytes to kSwissEmpty.
    memset(ctrl, static_cast<uint8_t>(kSwissEmpty), numCtrlBytes(capacity));
    ctrl[capacity] = kSwissSentinel;

    // Initialize slot values.
    if constexpr (!Traits::emptyValueIsZero) {
        for (unsigned i = 0; i < capacity; ++i)
            initializeBucket(slots[i]);
    }

    return { ctrl, slots };
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::deallocateTable(SwissCtrl* ctrl, ValueType* slots, unsigned capacity)
{
    for (unsigned i = 0; i < capacity; ++i)
        slots[i].~ValueType();
    HashTableMalloc::free(ctrl);
}

// --- expand / rehash / shrink ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::expand()
{
    unsigned newCapacity;
    if (!m_capacity) {
        unsigned minimumTableSize = KeyTraits::minimumTableSize;
        newCapacity = capacityForSlots(std::max(minimumTableSize, 1u));
    } else
        newCapacity = m_capacity * 2 + 1;

    rehash(newCapacity);
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::reinsert(ValueType&& value)
{
    unsigned hash = IdentityTranslatorType::hash(Extractor::extract(value));
    unsigned pos = findSlotForInsertion(hash);
    setCtrl(pos, h2(hash));
    ValueTraits::assignToEmpty(m_slots[pos], WTF::move(value));
    m_keyCount++;
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::rehash(unsigned newCapacity)
{
    internalCheckTableConsistencyExceptSize();

    unsigned oldCapacity = m_capacity;
    SwissCtrl* oldCtrl = m_ctrl;
    ValueType* oldSlots = m_slots;

    auto [newCtrl, newSlots] = allocateTable(newCapacity);
    m_ctrl = newCtrl;
    m_slots = newSlots;
    m_capacity = newCapacity;
    m_keyCount = 0;
    m_deletedCount = 0;

    for (unsigned i = 0; i < oldCapacity; ++i) {
        if (oldCtrl[i] >= 0) // occupied
            reinsert(WTF::move(oldSlots[i]));
        oldSlots[i].~ValueType();
    }

    if (oldCtrl)
        HashTableMalloc::free(oldCtrl);

    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::shrinkToBestSize()
{
    unsigned minimumCap = capacityForSlots(KeyTraits::minimumTableSize);
    rehash(std::max(minimumCap, computeBestTableSize(m_keyCount)));
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
constexpr unsigned SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::computeBestTableSize(unsigned keyCount)
{
    unsigned capacity = capacityForSlots(keyCount);

    if (shouldExpand(keyCount, capacity))
        capacity = capacity * 2 + 1;

    auto aboveThresholdForEagerExpansion = [](double loadFactor, unsigned keyCount, unsigned capacity) {
        double maxLoadRatio = loadFactor;
        double minLoadRatio = 1.0 / minLoad;
        double averageLoadRatio = std::midpoint(minLoadRatio, maxLoadRatio);
        double halfWayBetweenAverageAndMaxLoadRatio = std::midpoint(averageLoadRatio, maxLoadRatio);
        return keyCount >= capacity * halfWayBetweenAverageAndMaxLoadRatio;
    };

    constexpr double loadFactor = static_cast<double>(maxLoadNumerator) / maxLoadDenominator;
    if (aboveThresholdForEagerExpansion(loadFactor, keyCount, capacity))
        capacity = capacity * 2 + 1;

    unsigned minimumCap = capacityForSlots(KeyTraits::minimumTableSize);
    return std::max(capacity, minimumCap);
}

// --- Copy / Move / Swap ---

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::SwissHashTable(const SwissHashTable& other)
{
    if (!other.m_capacity || !other.m_keyCount)
        return;

    auto [ctrl, slots] = allocateTable(other.m_capacity);
    m_ctrl = ctrl;
    m_slots = slots;
    m_capacity = other.m_capacity;
    m_keyCount = 0;
    m_deletedCount = 0;

    for (unsigned i = 0; i < other.m_capacity; ++i) {
        if (other.m_ctrl[i] >= 0) {
            ValueType entry(other.m_slots[i]);
            reinsert(WTF::move(entry));
        }
    }
    internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::swap(SwissHashTable& other)
{
    using std::swap;
    invalidateIterators(this);
    invalidateIterators(&other);

    swap(m_ctrl, other.m_ctrl);
    swap(m_slots, other.m_slots);
    swap(m_capacity, other.m_capacity);
    swap(m_keyCount, other.m_keyCount);
    swap(m_deletedCount, other.m_deletedCount);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::operator=(const SwissHashTable& other) -> SwissHashTable&
{
    if (&other == this)
        return *this;

    SwissHashTable tmp(other);
    swap(tmp);
    return *this;
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::SwissHashTable(SwissHashTable&& other)
{
    invalidateIterators(&other);

    m_ctrl = std::exchange(other.m_ctrl, nullptr);
    m_slots = std::exchange(other.m_slots, nullptr);
    m_capacity = std::exchange(other.m_capacity, 0);
    m_keyCount = std::exchange(other.m_keyCount, 0);
    m_deletedCount = std::exchange(other.m_deletedCount, 0);

    internalCheckTableConsistency();
    other.internalCheckTableConsistency();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
inline auto SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::operator=(SwissHashTable&& other) -> SwissHashTable&
{
    SwissHashTable temp(WTF::move(other));
    swap(temp);
    return *this;
}

// --- Consistency checks ---

#if ASSERT_ENABLED

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::checkTableConsistency() const
{
    checkTableConsistencyExceptSize();
}

template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename SizePolicy, typename Malloc>
void SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SizePolicy, Malloc>::checkTableConsistencyExceptSize() const
{
    if (!m_ctrl)
        return;

    unsigned count = 0;
    unsigned deletedCount = 0;
    for (unsigned i = 0; i < m_capacity; ++i) {
        if (m_ctrl[i] >= 0) {
            auto& key = Extractor::extract(m_slots[i]);
            const_iterator it = find(key);
            ASSERT(m_slots + i == it.m_position);
            ++count;

            ValueCheck<Key>::checkConsistency(key);
        } else if (m_ctrl[i] == kSwissDeleted) {
            ++deletedCount;
        }
    }

    ASSERT(count == m_keyCount);
    ASSERT(deletedCount == m_deletedCount);
    ASSERT(m_ctrl[m_capacity] == kSwissSentinel);
}

#endif // ASSERT_ENABLED

// --- TableTraits ---

struct SwissHashTableTraits {
    template<typename Key, typename Value, typename Extractor, typename HashFunctions, typename Traits, typename KeyTraits, typename Malloc>
    using TableType = SwissHashTable<Key, Value, Extractor, HashFunctions, Traits, KeyTraits, SwissHashTableSizePolicy, Malloc>;
};

} // namespace WTF

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
