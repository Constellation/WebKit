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

#include "Test.h"
#include <wtf/OrderedHashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_OrderedHashSet, EmptySet)
{
    OrderedHashSet<int> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(0u, set.size());
    EXPECT_TRUE(set.begin() == set.end());
}

TEST(WTF_OrderedHashSet, BasicAddAndContains)
{
    OrderedHashSet<int> set;
    auto result = set.add(1);
    EXPECT_TRUE(result.isNewEntry);
    EXPECT_EQ(1u, set.size());

    auto result2 = set.add(1);
    EXPECT_FALSE(result2.isNewEntry);
    EXPECT_EQ(1u, set.size());

    EXPECT_TRUE(set.contains(1));
    EXPECT_FALSE(set.contains(2));
}

TEST(WTF_OrderedHashSet, InsertionOrderPreserved)
{
    OrderedHashSet<int> set;
    set.add(5);
    set.add(3);
    set.add(1);
    set.add(4);
    set.add(2);

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(5u, values.size());
    EXPECT_EQ(5, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(1, values[2]);
    EXPECT_EQ(4, values[3]);
    EXPECT_EQ(2, values[4]);
}

TEST(WTF_OrderedHashSet, InsertionOrderPreservedAfterDeletion)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);
    set.add(5);

    set.remove(3);

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(4u, values.size());
    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(2, values[1]);
    EXPECT_EQ(4, values[2]);
    EXPECT_EQ(5, values[3]);
}

TEST(WTF_OrderedHashSet, Remove)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    EXPECT_TRUE(set.remove(2));
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());
    EXPECT_FALSE(set.remove(999));
}

TEST(WTF_OrderedHashSet, RemoveByIterator)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto it = set.find(2);
    EXPECT_TRUE(set.remove(it));
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());
}

TEST(WTF_OrderedHashSet, Take)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto value = set.take(2);
    EXPECT_EQ(2, value);
    EXPECT_FALSE(set.contains(2));
    EXPECT_EQ(2u, set.size());

    auto missing = set.take(999);
    EXPECT_EQ(0, missing); // Default for missing
}

TEST(WTF_OrderedHashSet, TakeAny)
{
    OrderedHashSet<int> set;
    set.add(5);
    set.add(3);
    set.add(1);

    auto value = set.takeAny();
    EXPECT_EQ(5, value); // First in insertion order
    EXPECT_EQ(2u, set.size());
}

TEST(WTF_OrderedHashSet, Clear)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.clear();

    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(0u, set.size());
    EXPECT_TRUE(set.begin() == set.end());
}

TEST(WTF_OrderedHashSet, Swap)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2;
    set2.add(3);

    set1.swap(set2);

    EXPECT_EQ(1u, set1.size());
    EXPECT_TRUE(set1.contains(3));
    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
}

TEST(WTF_OrderedHashSet, CopyConstruction)
{
    OrderedHashSet<int> set1;
    set1.add(3);
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2(set1);

    EXPECT_EQ(3u, set2.size());

    Vector<int> values;
    for (auto& v : set2)
        values.append(v);

    EXPECT_EQ(3, values[0]);
    EXPECT_EQ(1, values[1]);
    EXPECT_EQ(2, values[2]);
}

TEST(WTF_OrderedHashSet, MoveConstruction)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2(WTFMove(set1));

    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
    EXPECT_TRUE(set1.isEmpty());
}

TEST(WTF_OrderedHashSet, CopyAssignment)
{
    OrderedHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    OrderedHashSet<int> set2;
    set2.add(9);
    set2 = set1;

    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
    EXPECT_FALSE(set2.contains(9));
}

TEST(WTF_OrderedHashSet, MoveAssignment)
{
    OrderedHashSet<int> set1;
    set1.add(1);

    OrderedHashSet<int> set2;
    set2.add(9);
    set2 = WTFMove(set1);

    EXPECT_EQ(1u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set1.isEmpty());
}

TEST(WTF_OrderedHashSet, RehashPreservesOrder)
{
    OrderedHashSet<int> set;
    Vector<int> expectedOrder;
    for (int i = 0; i < 100; ++i) {
        set.add(i);
        expectedOrder.append(i);
    }

    Vector<int> actualOrder;
    for (auto& v : set)
        actualOrder.append(v);

    EXPECT_EQ(expectedOrder, actualOrder);
}

TEST(WTF_OrderedHashSet, DeleteAndReinsert)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    set.remove(2);
    set.add(2); // Re-add goes at end

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(3u, values.size());
    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(2, values[2]); // Re-inserted at end
}

TEST(WTF_OrderedHashSet, ManyDeletesAndInserts)
{
    OrderedHashSet<int> set;
    for (int i = 0; i < 50; ++i)
        set.add(i);

    for (int i = 0; i < 50; i += 2)
        set.remove(i);

    EXPECT_EQ(25u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    for (int i = 0; i < 25; ++i)
        EXPECT_EQ(i * 2 + 1, values[i]);
}

TEST(WTF_OrderedHashSet, StringValues)
{
    OrderedHashSet<String> set;
    set.add("banana"_s);
    set.add("apple"_s);
    set.add("cherry"_s);

    Vector<String> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(3u, values.size());
    EXPECT_STREQ("banana", values[0].utf8().data());
    EXPECT_STREQ("apple", values[1].utf8().data());
    EXPECT_STREQ("cherry", values[2].utf8().data());
}

TEST(WTF_OrderedHashSet, InitializerList)
{
    OrderedHashSet<int> set { 5, 3, 1, 4, 2 };

    EXPECT_EQ(5u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(5, values[0]);
    EXPECT_EQ(3, values[1]);
    EXPECT_EQ(1, values[2]);
    EXPECT_EQ(4, values[3]);
    EXPECT_EQ(2, values[4]);
}

TEST(WTF_OrderedHashSet, AddAll)
{
    OrderedHashSet<int> set;
    set.add(1);

    Vector<int> toAdd { 2, 3, 4 };
    set.addAll(toAdd);

    EXPECT_EQ(4u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    EXPECT_EQ(1, values[0]);
    EXPECT_EQ(2, values[1]);
    EXPECT_EQ(3, values[2]);
    EXPECT_EQ(4, values[3]);
}

TEST(WTF_OrderedHashSet, RemoveIf)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);
    set.add(4);

    set.removeIf([](const int& v) { return v % 2 == 0; });

    EXPECT_EQ(2u, set.size());
    EXPECT_TRUE(set.contains(1));
    EXPECT_FALSE(set.contains(2));
    EXPECT_TRUE(set.contains(3));
    EXPECT_FALSE(set.contains(4));
}

TEST(WTF_OrderedHashSet, Find)
{
    OrderedHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    auto it = set.find(2);
    EXPECT_TRUE(it != set.end());
    EXPECT_EQ(2, *it);

    auto missing = set.find(999);
    EXPECT_TRUE(missing == set.end());
}

TEST(WTF_OrderedHashSet, ReserveCapacity)
{
    OrderedHashSet<int> set;
    set.reserveInitialCapacity(100);

    for (int i = 0; i < 100; ++i)
        set.add(i);

    EXPECT_EQ(100u, set.size());

    Vector<int> values;
    for (auto& v : set)
        values.append(v);

    for (int i = 0; i < 100; ++i)
        EXPECT_EQ(i, values[i]);
}

TEST(WTF_OrderedHashSet, StressTest)
{
    OrderedHashSet<int> set;
    // Insert 1000 elements
    for (int i = 0; i < 1000; ++i)
        set.add(i);

    EXPECT_EQ(1000u, set.size());

    // Remove every 3rd element
    for (int i = 0; i < 1000; i += 3)
        set.remove(i);

    // Verify remaining elements are in insertion order
    Vector<int> remaining;
    for (auto& v : set)
        remaining.append(v);

    int expectedIdx = 0;
    for (int i = 0; i < 1000; ++i) {
        if (i % 3 != 0) {
            EXPECT_EQ(i, remaining[expectedIdx]);
            ++expectedIdx;
        }
    }

    EXPECT_EQ(static_cast<unsigned>(expectedIdx), set.size());
}

} // namespace TestWebKitAPI
