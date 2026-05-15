/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include <numbers>
#include <wtf/text/AtomString.h>
#include <wtf/text/StringImpl.h>

namespace TestWebKitAPI {

TEST(WTF, AtomStringCreationFromLiteral)
{
    AtomString stringWithTemplate("Template Literal"_s);
    ASSERT_EQ(strlen("Template Literal"), stringWithTemplate.length());
    ASSERT_TRUE(stringWithTemplate == "Template Literal"_s);
    ASSERT_TRUE(stringWithTemplate.string().is8Bit());

    ASCIILiteral literal("Source literal");
    AtomString stringFromLiteral(literal);
    ASSERT_EQ(strlen("Source literal"), stringFromLiteral.length());
    ASSERT_TRUE(stringFromLiteral == "Source literal"_s);
    ASSERT_TRUE(stringFromLiteral.string().is8Bit());
    ASSERT_TRUE(std::bit_cast<uintptr_t>(stringFromLiteral.impl()->span8().data()) == std::bit_cast<uintptr_t>(literal.span().data()));
}

TEST(WTF, AtomStringCreationFromLiteralUniqueness)
{
    AtomString string1("Template Literal"_s);
    AtomString string2("Template Literal"_s);
    ASSERT_EQ(string1.impl(), string2.impl());

    AtomString string3("Template Literal"_s);
    ASSERT_EQ(string1.impl(), string3.impl());
}

TEST(WTF, AtomStringExistingHash)
{
    AtomString string1("Template Literal"_s);
    ASSERT_EQ(string1.existingHash(), string1.impl()->existingHash());
    AtomString string2;
    ASSERT_EQ(string2.existingHash(), 0u);
}

static inline const char* testAtomStringNumber(double number)
{
    static char testBuffer[100] = { };
    std::strncpy(testBuffer, AtomString::number(number).string().utf8().data(), 99);
    return testBuffer;
}

TEST(WTF, AtomStringCreationFromNullASCIILiteral)
{
    AtomString stringFromNull { ASCIILiteral() };
    ASSERT_TRUE(stringFromNull.isNull());
    ASSERT_TRUE(stringFromNull.isEmpty());

    AtomString stringFromEmpty(""_s);
    ASSERT_FALSE(stringFromEmpty.isNull());
    ASSERT_TRUE(stringFromEmpty.isEmpty());
}

TEST(WTF, AtomStringNumberDouble)
{
    using Limits = std::numeric_limits<double>;

    EXPECT_STREQ("Infinity", testAtomStringNumber(Limits::infinity()));
    EXPECT_STREQ("-Infinity", testAtomStringNumber(-Limits::infinity()));

    EXPECT_STREQ("NaN", testAtomStringNumber(-Limits::quiet_NaN()));

    EXPECT_STREQ("0", testAtomStringNumber(0));
    EXPECT_STREQ("0", testAtomStringNumber(-0));

    EXPECT_STREQ("2.2250738585072014e-308", testAtomStringNumber(Limits::min()));
    EXPECT_STREQ("-1.7976931348623157e+308", testAtomStringNumber(Limits::lowest()));
    EXPECT_STREQ("1.7976931348623157e+308", testAtomStringNumber(Limits::max()));

    EXPECT_STREQ("3.141592653589793", testAtomStringNumber(std::numbers::pi));
    EXPECT_STREQ("3.1415927410125732", testAtomStringNumber(std::numbers::pi_v<float>));
    EXPECT_STREQ("1.5707963267948966", testAtomStringNumber(piOverTwoDouble));
    EXPECT_STREQ("1.5707963705062866", testAtomStringNumber(piOverTwoFloat));
    EXPECT_STREQ("0.7853981633974483", testAtomStringNumber(piOverFourDouble));
    EXPECT_STREQ("0.7853981852531433", testAtomStringNumber(piOverFourFloat));

    EXPECT_STREQ("2.718281828459045", testAtomStringNumber(2.71828182845904523536028747135266249775724709369995));

    EXPECT_STREQ("299792458", testAtomStringNumber(299792458));

    EXPECT_STREQ("1.618033988749895", testAtomStringNumber(1.6180339887498948482));

    EXPECT_STREQ("1000", testAtomStringNumber(1e3));
    EXPECT_STREQ("10000000000", testAtomStringNumber(1e10));
    EXPECT_STREQ("100000000000000000000", testAtomStringNumber(1e20));
    EXPECT_STREQ("1e+21", testAtomStringNumber(1e21));
    EXPECT_STREQ("1e+30", testAtomStringNumber(1e30));

    EXPECT_STREQ("1100", testAtomStringNumber(1.1e3));
    EXPECT_STREQ("11000000000", testAtomStringNumber(1.1e10));
    EXPECT_STREQ("110000000000000000000", testAtomStringNumber(1.1e20));
    EXPECT_STREQ("1.1e+21", testAtomStringNumber(1.1e21));
    EXPECT_STREQ("1.1e+30", testAtomStringNumber(1.1e30));
}

TEST(WTF, AtomStringCanonicalize16BitLatin1ToOneByte)
{
    static constexpr char16_t chars[] = u"property_name_atom";
    constexpr size_t length = std::extent_v<decltype(chars)> - 1;

    Ref<StringImpl> sixteenBitLatin1 = StringImpl::create(std::span { chars, length });
    ASSERT_FALSE(sixteenBitLatin1->is8Bit());

    AtomString atom(sixteenBitLatin1.get());
    EXPECT_TRUE(atom.impl());
    EXPECT_TRUE(atom.impl()->is8Bit()) << "16-bit-Latin1 StringImpl must canonicalize to 8-bit atom";
    EXPECT_FALSE(sixteenBitLatin1->isAtom()) << "Original 16-bit StringImpl must not be marked atom";
    EXPECT_NE(atom.impl(), sixteenBitLatin1.ptr()) << "Atom must be a fresh StringImpl, not the 16-bit source";
    EXPECT_EQ(atom.length(), length);
}

TEST(WTF, AtomStringCrossEncodingDedup)
{
    static constexpr char16_t chars16[] = u"cross_encoding_dedup";
    constexpr size_t length = std::extent_v<decltype(chars16)> - 1;

    AtomString atomFrom8Bit("cross_encoding_dedup"_s);
    EXPECT_TRUE(atomFrom8Bit.impl()->is8Bit());

    Ref<StringImpl> sixteenBitLatin1 = StringImpl::create(std::span { chars16, length });
    ASSERT_FALSE(sixteenBitLatin1->is8Bit());
    AtomString atomFrom16Bit(sixteenBitLatin1.get());

    EXPECT_EQ(atomFrom8Bit.impl(), atomFrom16Bit.impl()) << "8-bit and 16-bit-Latin1 inputs must dedup to the same atom";
    EXPECT_TRUE(atomFrom16Bit.impl()->is8Bit());
}

TEST(WTF, AtomString16BitNonLatin1Preserved)
{
    static constexpr char16_t chars[] = u"café中"; // "café中"
    constexpr size_t length = std::extent_v<decltype(chars)> - 1;

    Ref<StringImpl> sixteenBit = StringImpl::create(std::span { chars, length });
    ASSERT_FALSE(sixteenBit->is8Bit());

    AtomString atom(sixteenBit.get());
    EXPECT_FALSE(atom.impl()->is8Bit()) << "16-bit with non-Latin1 content must stay 16-bit";
    EXPECT_EQ(atom.length(), length);
}

} // namespace TestWebKitAPI
