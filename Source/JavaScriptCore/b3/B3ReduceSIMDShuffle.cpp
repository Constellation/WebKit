/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2025-2026 the V8 project authors. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "B3ReduceSIMDShuffle.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlock.h"
#include "B3Const128Value.h"
#include "B3InsertionSetInlines.h"
#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3SIMDValue.h"
#include "B3UseCounts.h"
#include "B3ValueInlines.h"
#include "SIMDShuffle.h"
#include <wtf/HashMap.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

namespace {

namespace B3ReduceSIMDShuffleInternal {
static constexpr bool verbose = false;
}

static constexpr uint16_t allBytesDemanded = 0xFFFF;
static constexpr uint16_t low8BytesDemanded = 0x00FF;

static uint16_t demandedBytesForConsumer(Value* consumer, Value* producer)
{
    UNUSED_PARAM(producer);
    switch (consumer->opcode()) {
    case VectorExtendLow:
    case VectorConvertLow:
        return low8BytesDemanded;
    case VectorMulLow: {
        SIMDValue* simd = consumer->as<SIMDValue>();
        // All extmul_low variants read low 8 bytes.
        if (simd->simdLane() == SIMDLane::i64x2
            || simd->simdLane() == SIMDLane::i32x4
            || simd->simdLane() == SIMDLane::i16x8)
            return low8BytesDemanded;
        return allBytesDemanded;
    }
    case VectorExtractLane: {
        SIMDValue* simd = consumer->as<SIMDValue>();
        if (simd->immediate() != 0)
            return allBytesDemanded;
        switch (simd->simdLane()) {
        case SIMDLane::i64x2:
        case SIMDLane::f64x2:
            return low8BytesDemanded;
        case SIMDLane::i32x4:
        case SIMDLane::f32x4:
            return 0x000F;
        case SIMDLane::i16x8:
            return 0x0003;
        case SIMDLane::i8x16:
            return 0x0001;
        default:
            return allBytesDemanded;
        }
    }
    default:
        return allBytesDemanded;
    }
}

class SIMDShuffleReduction {
public:
    SIMDShuffleReduction(Procedure& procedure)
        : m_procedure(procedure)
    {
    }

    void run()
    {
        composeShuffles();
        narrowShuffles();
    }

private:
    void composeShuffles()
    {
        UseCounts useCounts(m_procedure);
        bool changed = true;
        unsigned iterations = 0;
        constexpr unsigned maxIterations = 4;

        while (changed && iterations < maxIterations) {
            changed = false;
            ++iterations;

            for (BasicBlock* block : m_procedure) {
                InsertionSet insertionSet(m_procedure);

                for (unsigned index = 0; index < block->size(); ++index) {
                    Value* value = block->at(index);
                    if (value->opcode() != VectorSwizzle)
                        continue;
                    if (value->numChildren() != 3)
                        continue;

                    Value* patternValue = value->child(2);
                    if (!patternValue->isConstant())
                        continue;

                    v128_t outerPattern = patternValue->as<Const128Value>()->value();

                    for (unsigned childIdx = 0; childIdx < 2; ++childIdx) {
                        Value* inner = value->child(childIdx);
                        if (inner->opcode() != VectorSwizzle)
                            continue;
                        if (inner->numChildren() != 2)
                            continue;
                        if (useCounts.numUses(inner) != 1)
                            continue;
                        if (!inner->child(1)->isConstant())
                            continue;

                        v128_t innerPattern = inner->child(1)->as<Const128Value>()->value();
                        auto composed = SIMDShuffle::composeShuffle(outerPattern, innerPattern, childIdx == 0);
                        if (!composed)
                            continue;

                        Value* innerSrc = inner->child(0);
                        Value* other = value->child(1 - childIdx);

                        Value* newChild0;
                        Value* newChild1;
                        if (childIdx == 0) {
                            newChild0 = innerSrc;
                            newChild1 = other;
                        } else {
                            newChild0 = other;
                            newChild1 = innerSrc;
                        }

                        v128_t newPattern = *composed;

                        // Check if composed result is unary (all indices from one side).
                        if (auto side = SIMDShuffle::isOnlyOneSideMask(newPattern)) {
                            Value* src;
                            v128_t unaryPattern = newPattern;
                            if (*side == 0)
                                src = newChild0;
                            else {
                                src = newChild1;
                                for (unsigned i = 0; i < 16; ++i)
                                    unaryPattern.u8x16[i] -= 16;
                            }
                            Value* newPat = m_procedure.addConstant(value->origin(), B3::V128, unaryPattern);
                            insertionSet.insertValue(index, newPat);
                            Value* newShuffle = insertionSet.insert<SIMDValue>(
                                index, value->origin(), VectorSwizzle, B3::V128,
                                SIMDLane::i8x16, SIMDSignMode::None, src, newPat);
                            value->replaceWithIdentity(newShuffle);
                        } else {
                            Value* newPat = m_procedure.addConstant(value->origin(), B3::V128, newPattern);
                            insertionSet.insertValue(index, newPat);
                            Value* newShuffle = insertionSet.insert<SIMDValue>(
                                index, value->origin(), VectorSwizzle, B3::V128,
                                SIMDLane::i8x16, SIMDSignMode::None, newChild0, newChild1, newPat);
                            value->replaceWithIdentity(newShuffle);
                        }

                        changed = true;
                        dataLogLnIf(B3ReduceSIMDShuffleInternal::verbose, "Composed shuffle: ", *value);
                        break;
                    }
                }

                insertionSet.execute(block);
            }

            if (changed)
                useCounts = UseCounts(m_procedure);
        }
    }

    void narrowShuffles()
    {
        UseCounts useCounts(m_procedure);

        // Compute union of demanded bytes for each VectorSwizzle.
        HashMap<Value*, uint16_t> demandMap;

        for (BasicBlock* block : m_procedure) {
            for (Value* value : *block) {
                for (unsigned i = 0; i < value->numChildren(); ++i) {
                    Value* child = value->child(i);
                    if (child->opcode() != VectorSwizzle || child->numChildren() != 2)
                        continue;

                    uint16_t demanded = demandedBytesForConsumer(value, child);
                    auto result = demandMap.add(child, demanded);
                    if (!result.isNewEntry)
                        result.iterator->value |= demanded;
                }
            }
        }

        // Narrow where demanded fits in low 8 bytes.
        for (BasicBlock* block : m_procedure) {
            InsertionSet insertionSet(m_procedure);

            for (unsigned index = 0; index < block->size(); ++index) {
                Value* value = block->at(index);
                if (value->opcode() != VectorSwizzle || value->numChildren() != 2)
                    continue;

                auto it = demandMap.find(value);
                if (it == demandMap.end())
                    continue;

                uint16_t demanded = it->value;
                if (demanded > low8BytesDemanded)
                    continue;

                if (!value->child(1)->isConstant())
                    continue;

                v128_t pattern = value->child(1)->as<Const128Value>()->value();
                SIMDShuffle::truncatePatternTo8(pattern);

                Value* newPat = m_procedure.addConstant(value->origin(), B3::V128, pattern);
                insertionSet.insertValue(index, newPat);

                Value* narrowed = insertionSet.insert<SIMDValue>(
                    index, value->origin(), VectorSwizzle8, B3::V128,
                    SIMDLane::i8x16, SIMDSignMode::None, value->child(0), newPat);
                value->replaceWithIdentity(narrowed);

                dataLogLnIf(B3ReduceSIMDShuffleInternal::verbose, "Narrowed shuffle to 8-byte: ", *narrowed);
            }

            insertionSet.execute(block);
        }
    }

    Procedure& m_procedure;
};

} // anonymous namespace

void reduceSIMDShuffle(Procedure& procedure)
{
    PhaseScope phaseScope(procedure, "reduceSIMDShuffle"_s);

    SIMDShuffleReduction reduction(procedure);
    reduction.run();
}

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
