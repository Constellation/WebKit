/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include <wtf/text/RapidHash.h>

#include <cstdio>

namespace TestWebKitAPI {

static const unsigned expected[256] = {
    15648162, 7379797, 2070420, 1145527, 8646054, 15241182, 9023636, 3432937, 5062235, 9411973, 8896458, 400898, 11288028, 15246497,
    16435911, 10710282, 11347848, 4926495, 5707730, 12461996, 15979074, 8192552, 4131547, 4233318, 12626894, 6755298, 7868599, 12823534,
    15827911, 14924241, 4759336, 1088812, 7488120, 15311191, 12013381, 16679728, 13118800, 13009161, 6484391, 4172168, 5167151, 817127,
    12403441, 2342506, 10144396, 11025206, 7062744, 15680250, 13612965, 869880, 4014223, 14373616, 4510356, 8595635, 5794747, 6413633,
    15160000, 8866527, 15428778, 12958164, 11046123, 13233833, 16651263, 8725559, 9176081, 14061736, 11344565, 5810756, 12458473, 3701732,
    6169408, 8744620, 13545725, 7498676, 10964582, 12211699, 6547798, 12410754, 15763191, 6742586, 12102147, 15835431, 7399542, 3934377,
    14165379, 16203007, 1429223, 8153632, 13217555, 9421910, 7722034, 804888, 13084468, 8451886, 8948988, 10345068, 15432129, 1274364,
    12502940, 4657399, 7753220, 2705153, 6018100, 9597385, 16276144, 7700322, 692044, 12527633, 3400946, 4513351, 11233477, 13209535,
    12920005, 12470278, 12416119, 5044722, 9607204, 6859191, 5691381, 15134985, 862804, 11678044, 9501460, 13797854, 9493874, 417362,
    11681400, 12943228, 6221477, 4274081, 9331847, 7330476, 9884564, 15862969, 6490954, 7551756, 13785547, 7802142, 12872755, 4615712,
    8904795, 1545520, 12927801, 5591092, 9366396, 11918163, 83670, 6325621, 8142363, 16462751, 3496810, 8595953, 1480256, 6721450,
    11516718, 11853227, 14404149, 12296203, 9395718, 7875681, 7096160, 3481091, 15127520, 8429186, 10728210, 10806655, 12191713, 14381579,
    14574049, 12323324, 2713101, 12030023, 6494449, 7096722, 10996011, 2863720, 789298, 8569965, 13120101, 13360858, 7175527, 76657,
    2755224, 2643920, 14137504, 1144911, 1672686, 10746076, 1935564, 4652328, 6334663, 15111256, 7944511, 4270370, 4440447, 4140331,
    14040329, 15929079, 13138253, 5570775, 13820680, 549163, 12115401, 14000058, 1581010, 13166429, 3102479, 12136319, 2626194, 10071173,
    14886108, 10021120, 964629, 11804913, 829378, 1004309, 6551740, 8857819, 15280313, 5586978, 1904222, 11159718, 3805985, 404066,
    3248840, 4377825, 9995210, 12087443, 2247276, 9838854, 15703402, 16102836, 11787949, 216832, 10266988, 4553076, 12253763, 924739,
    14137553, 13714506, 5107931, 4865914, 15687457, 13011562, 13781392, 15981974, 8615599, 793617, 12964706, 10511751, 7135570, 4958064,
    11359717, 16212004, 13049943, 13585204
};

TEST(WTF, RapidHasher)
{
    auto generateLatin1Array = [&](size_t size) {
        auto array = std::unique_ptr<Latin1Character[]>(new Latin1Character[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    auto generateUTF16Array = [&](size_t size) {
        auto array = std::unique_ptr<char16_t[]>(new char16_t[size]);
        for (size_t i = 0; i < size; i++)
            array[i] = i;
        return array;
    };

    unsigned max8Bit = std::numeric_limits<uint8_t>::max();
    for (size_t size = 0; size <= max8Bit; size++) {
        std::unique_ptr<const Latin1Character[]> arr1 = generateLatin1Array(size);
        std::unique_ptr<const char16_t[]> arr2 = generateUTF16Array(size);
        unsigned left = RapidHash::computeHashAndMaskTop8Bits(std::span { arr1.get(), size });
        unsigned right = RapidHash::computeHashAndMaskTop8Bits(std::span { arr2.get(), size });
        ASSERT_EQ(left, right);
        ASSERT_EQ(left, expected[size]);
    }
}

// Exercise the long-string branches in rapidhashCore (>16, >48, >96 bytes)
// across the full range of sizes used by real-world strings, and confirm the
// 8-bit vs 16-bit Latin1 invariant still holds.
TEST(WTF, RapidHasher_LongStringInvariant)
{
    constexpr size_t maxSize = 512;
    auto arr1Mut = std::unique_ptr<Latin1Character[]>(new Latin1Character[maxSize]);
    auto arr2Mut = std::unique_ptr<char16_t[]>(new char16_t[maxSize]);
    for (size_t i = 0; i < maxSize; ++i) {
        arr1Mut[i] = static_cast<Latin1Character>(i & 0xFF);
        arr2Mut[i] = static_cast<char16_t>(i & 0xFF);
    }
    const Latin1Character* arr1 = arr1Mut.get();
    const char16_t* arr2 = arr2Mut.get();
    for (size_t size = 0; size <= maxSize; ++size) {
        unsigned left = RapidHash::computeHashAndMaskTop8Bits(std::span { arr1, size });
        unsigned right = RapidHash::computeHashAndMaskTop8Bits(std::span { arr2, size });
        ASSERT_EQ(left, right) << "size=" << size;
    }
}

// A hash computed at compile time over a char16_t literal must equal both:
// (a) the hash computed at runtime over the same char16_t buffer, and
// (b) the hash of the equivalent Latin1 byte sequence (constexpr or runtime).
// Pre-fix, the constexpr 16-bit path used rapidhashRawBytes16 (len=2N) while
// runtime used rapidhashCompressed16 (len=N), so this would diverge.
TEST(WTF, RapidHasher_ConstexprMatchesRuntime)
{
    auto check = [](auto& literal8, auto& literal16) {
        constexpr size_t length = std::extent_v<std::remove_reference_t<decltype(literal8)>> - 1;
        static_assert(std::extent_v<std::remove_reference_t<decltype(literal16)>> - 1 == length);

        unsigned constexprHash16 = RapidHash::computeHashAndMaskTop8Bits<char16_t>(std::span { literal16, length });
        unsigned runtimeHash16 = RapidHash::computeHashAndMaskTop8Bits(std::span<const char16_t> { literal16, length });
        unsigned runtimeHash8 = RapidHash::computeHashAndMaskTop8Bits(std::span { reinterpret_cast<const Latin1Character*>(literal8), length });
        EXPECT_EQ(constexprHash16, runtimeHash16) << "length=" << length;
        EXPECT_EQ(constexprHash16, runtimeHash8) << "length=" << length;
    };

    {
        static constexpr char literal8[] = "x";
        static constexpr char16_t literal16[] = u"x";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "ab";
        static constexpr char16_t literal16[] = u"ab";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "abc";
        static constexpr char16_t literal16[] = u"abc";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "abcd";
        static constexpr char16_t literal16[] = u"abcd";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "abcde";
        static constexpr char16_t literal16[] = u"abcde";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "abcdefg";
        static constexpr char16_t literal16[] = u"abcdefg";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "abcdefgh";
        static constexpr char16_t literal16[] = u"abcdefgh";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "0123456789abcdef";
        static constexpr char16_t literal16[] = u"0123456789abcdef";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "0123456789abcdefg";
        static constexpr char16_t literal16[] = u"0123456789abcdefg";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "0123456789abcdef0123456789abcdef0123456789abcdef";
        static constexpr char16_t literal16[] = u"0123456789abcdef0123456789abcdef0123456789abcdef";
        check(literal8, literal16);
    }
    {
        static constexpr char literal8[] = "0123456789abcdef0123456789abcdef0123456789abcdef!";
        static constexpr char16_t literal16[] = u"0123456789abcdef0123456789abcdef0123456789abcdef!";
        check(literal8, literal16);
    }
    {
        // 64 chars
        static constexpr char literal8[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        static constexpr char16_t literal16[] = u"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
        check(literal8, literal16);
    }
    {
        // 100 chars
        static constexpr char literal8[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefABCD";
        static constexpr char16_t literal16[] = u"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefABCD";
        check(literal8, literal16);
    }
}

// Non-Latin1 char16_t hashed at constexpr must match runtime (no 8-bit
// equivalent; we only assert the constexpr-vs-runtime invariant).
TEST(WTF, RapidHasher_NonLatin1ConstexprMatchesRuntime)
{
    static constexpr char16_t mixed[] = u"a\u00A3\u1234b\u4E2D\u6587\u0041";
    constexpr size_t length = std::extent_v<decltype(mixed)> - 1;
    unsigned constexprHash = RapidHash::computeHashAndMaskTop8Bits<char16_t>(std::span { mixed, length });
    unsigned runtimeHash = RapidHash::computeHashAndMaskTop8Bits(std::span<const char16_t> { mixed, length });
    EXPECT_EQ(constexprHash, runtimeHash);
}

} // namespace TestWebKitAPI
