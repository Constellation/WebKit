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
#include <functional>
#include <wtf/RefPtr.h>
#include <wtf/SwissHashSet.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace TestWebKitAPI {

TEST(WTF_SwissHashSet, BasicAddContainsRemove)
{
    SwissHashSet<int> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_EQ(0u, set.size());

    set.add(1);
    set.add(2);
    set.add(3);

    EXPECT_EQ(3u, set.size());
    EXPECT_TRUE(set.contains(1));
    EXPECT_TRUE(set.contains(2));
    EXPECT_TRUE(set.contains(3));
    EXPECT_FALSE(set.contains(4));

    EXPECT_TRUE(set.remove(2));
    EXPECT_EQ(2u, set.size());
    EXPECT_FALSE(set.contains(2));

    EXPECT_FALSE(set.remove(2));
}

TEST(WTF_SwissHashSet, AddReturnValue)
{
    SwissHashSet<int> set;

    auto result = set.add(42);
    EXPECT_TRUE(result.isNewEntry);

    auto result2 = set.add(42);
    EXPECT_FALSE(result2.isNewEntry);
}

TEST(WTF_SwissHashSet, Clear)
{
    SwissHashSet<int> set;
    for (int i = 1; i <= 10; ++i)
        set.add(i);

    EXPECT_EQ(10u, set.size());
    set.clear();
    EXPECT_EQ(0u, set.size());
    EXPECT_TRUE(set.isEmpty());

    set.add(100);
    EXPECT_EQ(1u, set.size());
    EXPECT_TRUE(set.contains(100));
}

TEST(WTF_SwissHashSet, Iteration)
{
    SwissHashSet<int> set;
    set.add(10);
    set.add(20);
    set.add(30);

    Vector<int> values;
    for (auto value : set)
        values.append(value);

    std::sort(values.begin(), values.end());
    ASSERT_EQ(3u, values.size());
    EXPECT_EQ(10, values[0]);
    EXPECT_EQ(20, values[1]);
    EXPECT_EQ(30, values[2]);
}

TEST(WTF_SwissHashSet, MoveOnlyType)
{
    SwissHashSet<MoveOnly> set;

    set.add(MoveOnly(1));
    set.add(MoveOnly(2));
    set.add(MoveOnly(3));

    EXPECT_EQ(3u, set.size());
}

TEST(WTF_SwissHashSet, RefPtrType)
{
    RefLogger a("a");
    RefLogger b("b");

    SwissHashSet<RefPtr<RefLogger>> set;
    set.add(&a);
    set.add(&b);

    EXPECT_EQ(2u, set.size());
    EXPECT_TRUE(set.contains(&a));
    EXPECT_TRUE(set.contains(&b));
}

TEST(WTF_SwissHashSet, StringType)
{
    SwissHashSet<String> set;

    set.add("hello"_s);
    set.add("world"_s);
    set.add("foo"_s);

    EXPECT_EQ(3u, set.size());
    EXPECT_TRUE(set.contains("hello"_s));
    EXPECT_TRUE(set.contains("world"_s));
    EXPECT_TRUE(set.contains("foo"_s));
    EXPECT_FALSE(set.contains("bar"_s));
}

TEST(WTF_SwissHashSet, CopyConstruction)
{
    SwissHashSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);

    SwissHashSet<int> copy(set);
    EXPECT_EQ(3u, copy.size());
    EXPECT_TRUE(copy.contains(1));
    EXPECT_TRUE(copy.contains(2));
    EXPECT_TRUE(copy.contains(3));
}

TEST(WTF_SwissHashSet, MoveConstruction)
{
    SwissHashSet<int> set;
    set.add(1);
    set.add(2);

    SwissHashSet<int> moved(WTF::move(set));
    EXPECT_EQ(2u, moved.size());
    EXPECT_TRUE(moved.contains(1));
    EXPECT_TRUE(moved.contains(2));
    EXPECT_TRUE(set.isEmpty());
}

TEST(WTF_SwissHashSet, CopyAssignment)
{
    SwissHashSet<int> set;
    set.add(1);
    set.add(2);

    SwissHashSet<int> other;
    other.add(10);

    other = set;
    EXPECT_EQ(2u, other.size());
    EXPECT_TRUE(other.contains(1));
    EXPECT_TRUE(other.contains(2));
}

TEST(WTF_SwissHashSet, MoveAssignment)
{
    SwissHashSet<int> set;
    set.add(1);
    set.add(2);

    SwissHashSet<int> other;
    other.add(10);

    other = WTF::move(set);
    EXPECT_EQ(2u, other.size());
    EXPECT_TRUE(other.contains(1));
    EXPECT_TRUE(other.contains(2));
}

TEST(WTF_SwissHashSet, Swap)
{
    SwissHashSet<int> set1;
    set1.add(1);
    set1.add(2);

    SwissHashSet<int> set2;
    set2.add(10);

    set1.swap(set2);

    EXPECT_EQ(1u, set1.size());
    EXPECT_TRUE(set1.contains(10));
    EXPECT_EQ(2u, set2.size());
    EXPECT_TRUE(set2.contains(1));
    EXPECT_TRUE(set2.contains(2));
}

TEST(WTF_SwissHashSet, LargeScale)
{
    SwissHashSet<int> set;
    constexpr int count = 10000;

    for (int i = 1; i <= count; ++i)
        set.add(i);

    EXPECT_EQ(static_cast<unsigned>(count), set.size());

    for (int i = 1; i <= count; ++i)
        EXPECT_TRUE(set.contains(i));

    EXPECT_FALSE(set.contains(count + 1));

    // Remove even values.
    for (int i = 2; i <= count; i += 2)
        EXPECT_TRUE(set.remove(i));

    EXPECT_EQ(static_cast<unsigned>(count / 2), set.size());

    for (int i = 1; i <= count; ++i) {
        if (i % 2 == 0)
            EXPECT_FALSE(set.contains(i));
        else
            EXPECT_TRUE(set.contains(i));
    }
}

TEST(WTF_SwissHashSet, EmptySetCopy)
{
    SwissHashSet<int> empty;
    SwissHashSet<int> copy(empty);

    EXPECT_TRUE(copy.isEmpty());
}

TEST(WTF_SwissHashSet, ReserveInitialCapacity)
{
    SwissHashSet<int> set;
    set.reserveInitialCapacity(500);

    unsigned cap = set.capacity();
    EXPECT_GE(cap, 500u);

    for (int i = 1; i <= 500; ++i)
        set.add(i);

    EXPECT_EQ(cap, set.capacity());
    EXPECT_EQ(500u, set.size());
}

TEST(WTF_SwissHashSet, Random)
{
    SwissHashSet<int> set;
    set.add(10);
    set.add(20);
    set.add(30);

    auto it = set.random();
    ASSERT_TRUE(it != set.end());
    int val = *it;
    EXPECT_TRUE(val == 10 || val == 20 || val == 30);
}

TEST(WTF_SwissHashSet, EmptySet)
{
    SwissHashSet<int> set;
    EXPECT_TRUE(set.isEmpty());
    EXPECT_FALSE(set.contains(42));
    EXPECT_EQ(set.begin(), set.end());
}

TEST(WTF_SwissHashSet, SingleElement)
{
    SwissHashSet<int> set;
    set.add(42);

    EXPECT_EQ(1u, set.size());
    EXPECT_TRUE(set.contains(42));

    set.remove(42);
    EXPECT_TRUE(set.isEmpty());
}

TEST(WTF_SwissHashSet, AddRemoveStress)
{
    SwissHashSet<int> set;

    for (int round = 0; round < 10; ++round) {
        for (int i = 1; i <= 100; ++i)
            set.add(i);

        EXPECT_EQ(100u, set.size());

        for (int i = 2; i <= 100; i += 2)
            set.remove(i);

        EXPECT_EQ(50u, set.size());

        for (int i = 2; i <= 100; i += 2)
            set.add(i);

        EXPECT_EQ(100u, set.size());

        for (int i = 1; i <= 100; ++i)
            EXPECT_TRUE(set.contains(i));

        set.clear();
    }
}

TEST(WTF_SwissHashSet, InitializerList)
{
    SwissHashSet<int> set { 1, 2, 3, 4, 5 };

    EXPECT_EQ(5u, set.size());
    for (int i = 1; i <= 5; ++i)
        EXPECT_TRUE(set.contains(i));
}

TEST(WTF_SwissHashSet, UniquePtrWithRawPointerLookup)
{
    SwissHashSet<std::unique_ptr<int>> set;

    auto ptr = makeUniqueWithoutFastMallocCheck<int>(42);
    int* raw = ptr.get();
    set.add(WTF::move(ptr));

    EXPECT_EQ(1u, set.size());

    // Verify the element was added by iterating.
    bool found = false;
    for (auto& item : set) {
        if (item.get() == raw) {
            found = true;
            EXPECT_EQ(42, *item);
        }
    }
    EXPECT_TRUE(found);
}

TEST(WTF_SwissHashSet, RemoveAllThenReAdd)
{
    SwissHashSet<int> set;
    for (int i = 1; i <= 50; ++i)
        set.add(i);

    for (int i = 1; i <= 50; ++i)
        set.remove(i);

    EXPECT_TRUE(set.isEmpty());

    for (int i = 51; i <= 100; ++i)
        set.add(i);

    EXPECT_EQ(50u, set.size());
    for (int i = 51; i <= 100; ++i)
        EXPECT_TRUE(set.contains(i));
}

} // namespace TestWebKitAPI
