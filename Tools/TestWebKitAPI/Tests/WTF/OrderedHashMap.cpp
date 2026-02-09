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

#include "config.h"

#include "MoveOnly.h"
#include "Test.h"
#include <string>
#include <wtf/OrderedHashMap.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_OrderedHashMap, EmptyMap)
{
    OrderedHashMap<int, int> map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(map.begin() == map.end());
}

TEST(WTF_OrderedHashMap, BasicAddAndFind)
{
    OrderedHashMap<int, int> map;
    auto result = map.add(1, 100);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1, result.iterator->key);
    EXPECT_EQ(100, result.iterator->value);
    EXPECT_EQ(1u, map.size());

    auto result2 = map.add(1, 200);
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(100, result2.iterator->value); // Original value preserved
    EXPECT_EQ(1u, map.size());
}

TEST(WTF_OrderedHashMap, Set)
{
    OrderedHashMap<int, int> map;
    map.set(1, 100);
    EXPECT_EQ(100, map.get(1));

    map.set(1, 200);
    EXPECT_EQ(200, map.get(1)); // Value updated
    EXPECT_EQ(1u, map.size()); // Size unchanged
}

TEST(WTF_OrderedHashMap, Ensure)
{
    OrderedHashMap<int, int> map;
    auto result1 = map.ensure(1, []() { return 100; });
    EXPECT_TRUE(result1.isNewEntry);
    EXPECT_EQ(100, result1.iterator->value);

    auto result2 = map.ensure(1, []() { return 200; });
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(100, result2.iterator->value); // Functor not called
}

TEST(WTF_OrderedHashMap, Contains)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    EXPECT_TRUE(map.contains(1));
    EXPECT_TRUE(map.contains(2));
    EXPECT_FALSE(map.contains(3));
}

TEST(WTF_OrderedHashMap, Get)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    EXPECT_EQ(100, map.get(1));
    EXPECT_EQ(0, map.get(999)); // Default for missing key
}

TEST(WTF_OrderedHashMap, GetOptional)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    auto result = map.getOptional(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(100, *result);

    auto missing = map.getOptional(999);
    EXPECT_FALSE(missing.has_value());
}

TEST(WTF_OrderedHashMap, InsertionOrderPreserved)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(4, 400);
    map.add(1, 999); // Duplicate, should not change order
    map.add(5, 500);
    map.add(2, 200);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(5u, keys.size());
    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(4, keys[2]);
    EXPECT_EQ(5, keys[3]);
    EXPECT_EQ(2, keys[4]);
}

TEST(WTF_OrderedHashMap, InsertionOrderPreservedAfterDeletion)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);
    map.add(4, 400);
    map.add(5, 500);

    map.remove(3);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(4u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
    EXPECT_EQ(4, keys[2]);
    EXPECT_EQ(5, keys[3]);
}

TEST(WTF_OrderedHashMap, SetDoesNotChangeOrder)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    map.set(2, 999);

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]); // Same position
    EXPECT_EQ(3, keys[2]);
    EXPECT_EQ(999, map.get(2)); // Value updated
}

TEST(WTF_OrderedHashMap, RemoveByKey)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    EXPECT_TRUE(map.remove(1));
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());
    EXPECT_FALSE(map.remove(999)); // Non-existent key
}

TEST(WTF_OrderedHashMap, RemoveByIterator)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    auto it = map.find(1);
    EXPECT_TRUE(map.remove(it));
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());
}

TEST(WTF_OrderedHashMap, Take)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    auto value = map.take(1);
    EXPECT_EQ(100, value);
    EXPECT_FALSE(map.contains(1));
    EXPECT_EQ(1u, map.size());

    auto missing = map.take(999);
    EXPECT_EQ(0, missing); // Default for missing
}

TEST(WTF_OrderedHashMap, TakeOptional)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);

    auto result = map.takeOptional(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(100, *result);
    EXPECT_FALSE(map.contains(1));

    auto missing = map.takeOptional(999);
    EXPECT_FALSE(missing.has_value());
}

TEST(WTF_OrderedHashMap, TakeFirst)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    auto value = map.takeFirst();
    EXPECT_EQ(300, value); // First in insertion order
    EXPECT_EQ(2u, map.size());
}

TEST(WTF_OrderedHashMap, Clear)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.clear();

    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(0u, map.size());
    EXPECT_TRUE(map.begin() == map.end());
}

TEST(WTF_OrderedHashMap, Swap)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2;
    map2.add(3, 300);

    map1.swap(map2);

    EXPECT_EQ(1u, map1.size());
    EXPECT_TRUE(map1.contains(3));
    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
}

TEST(WTF_OrderedHashMap, CopyConstruction)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);
    map1.add(3, 300);

    OrderedHashMap<int, int> map2(map1);

    EXPECT_EQ(3u, map2.size());

    // Check insertion order is preserved in copy
    Vector<int> keys;
    for (auto& pair : map2)
        keys.append(pair.key);

    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
    EXPECT_EQ(3, keys[2]);
}

TEST(WTF_OrderedHashMap, MoveConstruction)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2(WTFMove(map1));

    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
    EXPECT_TRUE(map1.isEmpty());
}

TEST(WTF_OrderedHashMap, CopyAssignment)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);
    map1.add(2, 200);

    OrderedHashMap<int, int> map2;
    map2.add(9, 900);
    map2 = map1;

    EXPECT_EQ(2u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map2.contains(2));
    EXPECT_FALSE(map2.contains(9));
}

TEST(WTF_OrderedHashMap, MoveAssignment)
{
    OrderedHashMap<int, int> map1;
    map1.add(1, 100);

    OrderedHashMap<int, int> map2;
    map2.add(9, 900);
    map2 = WTFMove(map1);

    EXPECT_EQ(1u, map2.size());
    EXPECT_TRUE(map2.contains(1));
    EXPECT_TRUE(map1.isEmpty());
}

TEST(WTF_OrderedHashMap, KeysIteration)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    Vector<int> keys;
    for (auto& key : map.keys())
        keys.append(key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(2, keys[2]);
}

TEST(WTF_OrderedHashMap, ValuesIteration)
{
    OrderedHashMap<int, int> map;
    map.add(3, 300);
    map.add(1, 100);
    map.add(2, 200);

    Vector<int> values;
    for (auto& value : map.values())
        values.append(value);

    EXPECT_EQ(3u, values.size());
    EXPECT_EQ(300, values[0]);
    EXPECT_EQ(100, values[1]);
    EXPECT_EQ(200, values[2]);
}

TEST(WTF_OrderedHashMap, RehashPreservesOrder)
{
    OrderedHashMap<int, int> map;
    // Insert enough entries to trigger rehashing
    Vector<int> expectedOrder;
    for (int i = 0; i < 100; ++i) {
        map.add(i, i * 10);
        expectedOrder.append(i);
    }

    Vector<int> actualOrder;
    for (auto& pair : map)
        actualOrder.append(pair.key);

    EXPECT_EQ(expectedOrder, actualOrder);
}

TEST(WTF_OrderedHashMap, DeleteAndReinsert)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    map.remove(2);
    map.add(2, 250); // Re-add goes at end

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(3, keys[1]);
    EXPECT_EQ(2, keys[2]); // Re-inserted at end
    EXPECT_EQ(250, map.get(2));
}

TEST(WTF_OrderedHashMap, ManyDeletesAndInserts)
{
    OrderedHashMap<int, int> map;
    for (int i = 0; i < 50; ++i)
        map.add(i, i);

    // Delete even numbers
    for (int i = 0; i < 50; i += 2)
        map.remove(i);

    EXPECT_EQ(25u, map.size());

    // Check remaining are in order
    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    for (int i = 0; i < 25; ++i)
        EXPECT_EQ(i * 2 + 1, keys[i]);
}

TEST(WTF_OrderedHashMap, StringKeys)
{
    OrderedHashMap<String, int> map;
    map.add("banana"_s, 2);
    map.add("apple"_s, 1);
    map.add("cherry"_s, 3);

    Vector<String> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3u, keys.size());
    EXPECT_STREQ("banana", keys[0].utf8().data());
    EXPECT_STREQ("apple", keys[1].utf8().data());
    EXPECT_STREQ("cherry", keys[2].utf8().data());
}

TEST(WTF_OrderedHashMap, InitializerList)
{
    OrderedHashMap<int, int> map {
        { 3, 300 },
        { 1, 100 },
        { 2, 200 }
    };

    EXPECT_EQ(3u, map.size());

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    EXPECT_EQ(3, keys[0]);
    EXPECT_EQ(1, keys[1]);
    EXPECT_EQ(2, keys[2]);
}

TEST(WTF_OrderedHashMap, RemoveIf)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);
    map.add(4, 400);

    map.removeIf([](auto& pair) { return pair.key % 2 == 0; });

    EXPECT_EQ(2u, map.size());
    EXPECT_TRUE(map.contains(1));
    EXPECT_FALSE(map.contains(2));
    EXPECT_TRUE(map.contains(3));
    EXPECT_FALSE(map.contains(4));
}

TEST(WTF_OrderedHashMap, ReserveCapacity)
{
    OrderedHashMap<int, int> map;
    map.reserveInitialCapacity(100);

    for (int i = 0; i < 100; ++i)
        map.add(i, i);

    EXPECT_EQ(100u, map.size());

    Vector<int> keys;
    for (auto& pair : map)
        keys.append(pair.key);

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(i, keys[i]);
}

TEST(WTF_OrderedHashMap, ConstIteration)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);

    const auto& constMap = map;
    Vector<int> keys;
    for (auto& pair : constMap)
        keys.append(pair.key);

    EXPECT_EQ(2u, keys.size());
    EXPECT_EQ(1, keys[0]);
    EXPECT_EQ(2, keys[1]);
}

TEST(WTF_OrderedHashMap, FindReturnsCorrectIterator)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);
    map.add(2, 200);
    map.add(3, 300);

    auto it = map.find(2);
    EXPECT_TRUE(it != map.end());
    EXPECT_EQ(2, it->key);
    EXPECT_EQ(200, it->value);

    auto missing = map.find(999);
    EXPECT_TRUE(missing == map.end());
}

TEST(WTF_OrderedHashMap, IteratorComparison)
{
    OrderedHashMap<int, int> map;
    map.add(1, 100);

    EXPECT_TRUE(map.begin() != map.end());
    EXPECT_FALSE(map.begin() == map.end());

    OrderedHashMap<int, int>::const_iterator constBegin = map.begin();
    EXPECT_TRUE(constBegin == map.begin());
    EXPECT_TRUE(constBegin != map.end());
}

} // namespace TestWebKitAPI
