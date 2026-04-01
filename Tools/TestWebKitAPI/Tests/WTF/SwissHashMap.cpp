/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "Counters.h"
#include "MoveOnly.h"
#include "RefLogger.h"
#include "Test.h"
#include <string>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/SwissHashMap.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

// Note: For int keys, HashTraits<int> uses 0 as empty value and -1 as deleted value.
// Therefore all int keys must avoid 0 and -1.
typedef SwissHashMap<int, int> IntIntMap;

TEST(WTF_SwissHashMap, HashTableIteratorComparison)
{
    IntIntMap map;
    map.add(1, 2);
    ASSERT_TRUE(map.begin() != map.end());
    ASSERT_FALSE(map.begin() == map.end());

    IntIntMap::const_iterator begin = map.begin();
    ASSERT_TRUE(begin == map.begin());
    ASSERT_TRUE(map.begin() == begin);
    ASSERT_TRUE(begin != map.end());
    ASSERT_TRUE(map.end() != begin);
    ASSERT_FALSE(begin != map.begin());
    ASSERT_FALSE(map.begin() != begin);
    ASSERT_FALSE(begin == map.end());
    ASSERT_FALSE(map.end() == begin);
}

TEST(WTF_SwissHashMap, BasicAddAndFind)
{
    IntIntMap map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());

    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    EXPECT_FALSE(map.isEmpty());
    EXPECT_EQ(3u, map.size());

    EXPECT_EQ(10, map.get(1));
    EXPECT_EQ(20, map.get(2));
    EXPECT_EQ(30, map.get(3));
    EXPECT_EQ(0, map.get(4));

    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(2));
    EXPECT_TRUE(map.contains(3));
    EXPECT_FALSE(map.contains(4));
}

TEST(WTF_SwissHashMap, AddReturnValue)
{
    IntIntMap map;

    auto result = map.add(1, 10);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.iterator->key);
    EXPECT_EQ(10, result.iterator->value);

    auto result2 = map.add(1, 20);
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1, result2.iterator->key);
    EXPECT_EQ(10, result2.iterator->value); // Unchanged
}

TEST(WTF_SwissHashMap, Set)
{
    IntIntMap map;

    map.set(1, 10);
    EXPECT_EQ(10, map.get(1));

    map.set(1, 20);
    EXPECT_EQ(20, map.get(1));
}

TEST(WTF_SwissHashMap, Ensure)
{
    IntIntMap map;

    auto result = map.ensure(1, [] { return 10; });
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(10, result.iterator->value);

    auto result2 = map.ensure(1, [] { return 20; });
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(10, result2.iterator->value); // Unchanged
}

TEST(WTF_SwissHashMap, Remove)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    EXPECT_TRUE(map.remove(2));
    EXPECT_EQ(2u, map.size());
    EXPECT_FALSE(map.contains(2));
    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(3));

    EXPECT_FALSE(map.remove(2));
}

TEST(WTF_SwissHashMap, RemoveByIterator)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    auto it = map.find(2);
    ASSERT_TRUE(it != map.end());
    map.remove(it);

    EXPECT_EQ(2u, map.size());
    EXPECT_FALSE(map.contains(2));
}

TEST(WTF_SwissHashMap, Clear)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    EXPECT_EQ(3u, map.size());
    map.clear();
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(map.isEmpty());

    // Re-add after clear.
    map.add(4, 40);
    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(40, map.get(4));
}

TEST(WTF_SwissHashMap, Iteration)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    Vector<int> keys;
    Vector<int> values;
    for (auto& pair : map) {
        keys.append(pair.key);
        values.append(pair.value);
    }

    std::sort(keys.begin(), keys.end());
    std::sort(values.begin(), values.end());

    ASSERT_EQ(3u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
    EXPECT_EQ(3, keys[2]);
    EXPECT_EQ(10, values[0]);
    EXPECT_EQ(20, values[1]);
    EXPECT_EQ(30, values[2]);
}

TEST(WTF_SwissHashMap, MoveOnlyValues)
{
    SwissHashMap<int, MoveOnly> map;

    MoveOnly val(42);
    map.add(1, WTF::move(val));

    auto it = map.find(1);
    ASSERT_TRUE(it != map.end());
    EXPECT_EQ(42u, it->value.value());
}

TEST(WTF_SwissHashMap, MoveOnlyKeys)
{
    SwissHashMap<MoveOnly, int> map;

    map.add(MoveOnly(1), 10);
    map.add(MoveOnly(2), 20);

    EXPECT_EQ(2u, map.size());
}

TEST(WTF_SwissHashMap, UniquePtrValues)
{
    SwissHashMap<int, std::unique_ptr<int>> map;

    map.add(1, makeUniqueWithoutFastMallocCheck<int>(100));
    map.add(2, makeUniqueWithoutFastMallocCheck<int>(200));

    EXPECT_EQ(100, *map.get(1));
    EXPECT_EQ(200, *map.get(2));
    EXPECT_EQ(nullptr, map.get(3));
}

TEST(WTF_SwissHashMap, RefPtrValues)
{
    RefLogger a("a");
    RefLogger b("b");

    SwissHashMap<int, RefPtr<RefLogger>> map;
    map.add(1, &a);
    map.add(2, &b);

    EXPECT_EQ(&a, map.get(1));
    EXPECT_EQ(&b, map.get(2));
}

TEST(WTF_SwissHashMap, CopyConstruction)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    IntIntMap copy(map);
    EXPECT_EQ(2u, copy.size());
    EXPECT_EQ(10, copy.get(1));
    EXPECT_EQ(20, copy.get(2));
}

TEST(WTF_SwissHashMap, MoveConstruction)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    IntIntMap moved(WTF::move(map));
    EXPECT_EQ(2u, moved.size());
    EXPECT_EQ(10, moved.get(1));
    EXPECT_EQ(20, moved.get(2));
    EXPECT_TRUE(map.isEmpty());
}

TEST(WTF_SwissHashMap, CopyAssignment)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    IntIntMap other;
    other.add(3, 30);

    other = map;
    EXPECT_EQ(2u, other.size());
    EXPECT_EQ(10, other.get(1));
    EXPECT_EQ(20, other.get(2));
}

TEST(WTF_SwissHashMap, MoveAssignment)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    IntIntMap other;
    other.add(3, 30);

    other = WTF::move(map);
    EXPECT_EQ(2u, other.size());
    EXPECT_EQ(10, other.get(1));
    EXPECT_EQ(20, other.get(2));
}

TEST(WTF_SwissHashMap, Swap)
{
    IntIntMap map1;
    map1.add(1, 10);
    map1.add(2, 20);

    IntIntMap map2;
    map2.add(3, 30);

    map1.swap(map2);

    EXPECT_EQ(1u, map1.size());
    EXPECT_EQ(30, map1.get(3));
    EXPECT_EQ(2u, map2.size());
    EXPECT_EQ(10, map2.get(1));
    EXPECT_EQ(20, map2.get(2));
}

TEST(WTF_SwissHashMap, ReserveInitialCapacity)
{
    IntIntMap map;
    map.reserveInitialCapacity(1000);

    unsigned cap = map.capacity();
    EXPECT_GE(cap, 1000u);

    // Keys 1..1000 (avoid 0 which is the empty value for int traits).
    for (int i = 1; i <= 1000; ++i)
        map.add(i, i * 10);

    EXPECT_EQ(1000u, map.size());

    for (int i = 1; i <= 1000; ++i)
        EXPECT_EQ(i * 10, map.get(i));
}

TEST(WTF_SwissHashMap, LargeScale)
{
    IntIntMap map;
    constexpr int count = 10000;

    // Use keys 1..count to avoid 0 (empty) and -1 (deleted).
    for (int i = 1; i <= count; ++i)
        map.add(i, i * 3);

    EXPECT_EQ(static_cast<unsigned>(count), map.size());

    for (int i = 1; i <= count; ++i)
        EXPECT_EQ(i * 3, map.get(i));

    // Remove even keys.
    for (int i = 2; i <= count; i += 2)
        EXPECT_TRUE(map.remove(i));

    EXPECT_EQ(static_cast<unsigned>(count / 2), map.size());

    // Verify remaining.
    for (int i = 1; i <= count; ++i) {
        if (i % 2 == 0)
            EXPECT_FALSE(map.contains(i));
        else
            EXPECT_EQ(i * 3, map.get(i));
    }

    // Re-add removed entries.
    for (int i = 2; i <= count; i += 2)
        map.add(i, i * 5);

    EXPECT_EQ(static_cast<unsigned>(count), map.size());

    for (int i = 1; i <= count; ++i) {
        if (i % 2 == 0)
            EXPECT_EQ(i * 5, map.get(i));
        else
            EXPECT_EQ(i * 3, map.get(i));
    }
}

TEST(WTF_SwissHashMap, RemoveIf)
{
    IntIntMap map;
    for (int i = 1; i <= 100; ++i)
        map.add(i, i * 10);

    map.removeIf([](auto& pair) {
        return pair.value > 500;
    });

    EXPECT_EQ(50u, map.size());

    for (int i = 1; i <= 100; ++i) {
        if (i <= 50)
            EXPECT_EQ(i * 10, map.get(i));
        else
            EXPECT_FALSE(map.contains(i));
    }
}

TEST(WTF_SwissHashMap, StringKeys)
{
    SwissHashMap<String, int> map;

    map.add("hello"_s, 1);
    map.add("world"_s, 2);
    map.add("foo"_s, 3);

    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(1, map.get("hello"_s));
    EXPECT_EQ(2, map.get("world"_s));
    EXPECT_EQ(3, map.get("foo"_s));
    EXPECT_EQ(0, map.get("bar"_s));
}

TEST(WTF_SwissHashMap, EmptyMap)
{
    IntIntMap map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());
    EXPECT_FALSE(map.contains(42));
    EXPECT_EQ(map.begin(), map.end());
    EXPECT_EQ(0, map.get(42));
}

TEST(WTF_SwissHashMap, SingleElement)
{
    IntIntMap map;
    map.add(42, 100);

    EXPECT_EQ(1u, map.size());
    EXPECT_TRUE(map.contains(42));
    EXPECT_EQ(100, map.get(42));

    map.remove(42);
    EXPECT_TRUE(map.isEmpty());
}

TEST(WTF_SwissHashMap, Random)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);
    map.add(3, 30);

    auto it = map.random();
    ASSERT_TRUE(it != map.end());
    EXPECT_TRUE(it->key >= 1 && it->key <= 3);
}

TEST(WTF_SwissHashMap, InitializerList)
{
    SwissHashMap<int, int> map {
        { 1, 10 },
        { 2, 20 },
        { 3, 30 },
    };

    EXPECT_EQ(3u, map.size());
    EXPECT_EQ(10, map.get(1));
    EXPECT_EQ(20, map.get(2));
    EXPECT_EQ(30, map.get(3));
}

TEST(WTF_SwissHashMap, Take)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    auto taken = map.take(1);
    EXPECT_EQ(10, taken);
    EXPECT_EQ(1u, map.size());
    EXPECT_FALSE(map.contains(1));

    auto notTaken = map.take(999);
    EXPECT_EQ(0, notTaken);
}

TEST(WTF_SwissHashMap, AddRemoveAddStress)
{
    IntIntMap map;

    // Stress test: add and remove in various patterns to exercise tombstone handling.
    for (int round = 0; round < 10; ++round) {
        for (int i = 1; i <= 100; ++i)
            map.add(i, i + round);

        EXPECT_EQ(100u, map.size());

        for (int i = 2; i <= 100; i += 2)
            map.remove(i);

        EXPECT_EQ(50u, map.size());

        for (int i = 2; i <= 100; i += 2)
            map.add(i, i + round + 1000);

        EXPECT_EQ(100u, map.size());

        // Verify all entries exist.
        for (int i = 1; i <= 100; ++i) {
            EXPECT_TRUE(map.contains(i));
            if (i % 2 == 0)
                EXPECT_EQ(i + round + 1000, map.get(i));
            else
                EXPECT_EQ(i + round, map.get(i));
        }

        map.clear();
    }
}

TEST(WTF_SwissHashMap, RefValues)
{
    RefLogger a("a");

    {
        SwissHashMap<int, Ref<RefLogger>> map;
        map.add(1, a);
        map.add(2, a);

        EXPECT_EQ(&a, map.get(1));
        EXPECT_EQ(&a, map.get(2));
    }
}

TEST(WTF_SwissHashMap, KeysAndValues)
{
    IntIntMap map;
    map.add(1, 10);
    map.add(2, 20);

    Vector<int> keys;
    for (auto key : map.keys())
        keys.append(key);
    std::sort(keys.begin(), keys.end());
    ASSERT_EQ(2u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);

    Vector<int> values;
    for (auto value : map.values())
        values.append(value);
    std::sort(values.begin(), values.end());
    ASSERT_EQ(2u, values.size());
    EXPECT_EQ(10, values[0]);
    EXPECT_EQ(20, values[1]);
}

TEST(WTF_SwissHashMap, EmptyMapCopy)
{
    IntIntMap empty;
    IntIntMap copy(empty);

    EXPECT_TRUE(copy.isEmpty());
    EXPECT_EQ(0u, copy.size());
}

TEST(WTF_SwissHashMap, SelfAssignment)
{
    IntIntMap map;
    map.add(1, 10);

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
    map = map;
#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

    EXPECT_EQ(1u, map.size());
    EXPECT_EQ(10, map.get(1));
}

TEST(WTF_SwissHashMap, RemoveAllThenReAdd)
{
    IntIntMap map;
    for (int i = 1; i <= 50; ++i)
        map.add(i, i);

    for (int i = 1; i <= 50; ++i)
        map.remove(i);

    EXPECT_TRUE(map.isEmpty());

    // Re-add to exercise rehashing of a table full of tombstones.
    for (int i = 101; i <= 150; ++i)
        map.add(i, i);

    EXPECT_EQ(50u, map.size());
    for (int i = 101; i <= 150; ++i)
        EXPECT_EQ(i, map.get(i));
}

TEST(WTF_SwissHashMap, CapacityBoundary)
{
    // Test behavior around load factor boundaries.
    IntIntMap map;
    map.reserveInitialCapacity(15);
    unsigned cap = map.capacity();

    // Add entries up to the 87.5% load factor boundary.
    unsigned maxBeforeRehash = cap * 7 / 8;
    for (unsigned i = 1; i <= maxBeforeRehash; ++i)
        map.add(i, i);

    EXPECT_EQ(maxBeforeRehash, map.size());
}

} // namespace TestWebKitAPI
