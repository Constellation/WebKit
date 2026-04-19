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
#include "B3RangeAnalysis.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3Dominators.h"
#include "B3InsertionSet.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include "WasmLimits.h"
#include <wtf/IndexMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

namespace {

namespace B3RangeAnalysisInternal {
static constexpr bool verbose = false;
}

// Closed signed Int32 interval [min, max]. Stored as int64_t so that
// transfer-function arithmetic (Add/Sub of two Int32 ranges) cannot wrap;
// any result that escapes the Int32 range is collapsed to TOP via fitsInt32().
struct Range {
    int64_t min { std::numeric_limits<int32_t>::min() };
    int64_t max { std::numeric_limits<int32_t>::max() };

    static Range top()
    {
        return Range { std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() };
    }

    static Range constant(int32_t c)
    {
        return Range { c, c };
    }

    bool isTop() const
    {
        return min == std::numeric_limits<int32_t>::min() && max == std::numeric_limits<int32_t>::max();
    }

    bool fitsInt32() const
    {
        return min >= std::numeric_limits<int32_t>::min() && max <= std::numeric_limits<int32_t>::max();
    }

    Range meet(Range other) const
    {
        Range r;
        r.min = std::max(min, other.min);
        r.max = std::min(max, other.max);
        if (r.min > r.max)
            return top();
        return r;
    }

    void dump(PrintStream& out) const
    {
        out.print("[", min, ", ", max, "]");
    }
};

class RangeAnalysis {
public:
    RangeAnalysis(Procedure& proc)
        : m_proc(proc)
        , m_insertionSet(proc)
        , m_baseRange(proc.values().size())
        , m_refinements(proc.values().size())
    {
    }

    bool run()
    {
        computeBaseRanges();
        collectRefinements();
        return foldComparisons();
    }

private:
    static bool isComparisonOpcode(Opcode op)
    {
        switch (op) {
        case Equal:
        case NotEqual:
        case LessThan:
        case GreaterThan:
        case LessEqual:
        case GreaterEqual:
        case Above:
        case Below:
        case AboveEqual:
        case BelowEqual:
            return true;
        default:
            return false;
        }
    }

    static Opcode invertComparison(Opcode op)
    {
        switch (op) {
        case Equal: return NotEqual;
        case NotEqual: return Equal;
        case LessThan: return GreaterEqual;
        case LessEqual: return GreaterThan;
        case GreaterThan: return LessEqual;
        case GreaterEqual: return LessThan;
        case Above: return BelowEqual;
        case AboveEqual: return Below;
        case Below: return AboveEqual;
        case BelowEqual: return Above;
        default:
            ASSERT_NOT_REACHED();
            return op;
        }
    }

    Range lookupBase(Value* v) const
    {
        return m_baseRange[v];
    }

    Range transfer(Value* v) const
    {
        switch (v->opcode()) {
        case Const32:
            return Range::constant(v->asInt32());
        case Add: {
            if (v->numChildren() != 2)
                return Range::top();
            Range a = lookupBase(v->child(0));
            Range b = lookupBase(v->child(1));
            Range r { a.min + b.min, a.max + b.max };
            if (!r.fitsInt32())
                return Range::top();
            return r;
        }
        case Sub: {
            if (v->numChildren() != 2)
                return Range::top();
            Range a = lookupBase(v->child(0));
            Range b = lookupBase(v->child(1));
            Range r { a.min - b.max, a.max - b.min };
            if (!r.fitsInt32())
                return Range::top();
            return r;
        }
        case WasmArrayLength:
            // A wasm array's length in bytes is bounded by Wasm::maxArraySizeInBytes,
            // and the length in elements is at most the byte bound.
            return Range { 0, static_cast<int32_t>(Wasm::maxArraySizeInBytes) };
        case Phi:
            // Conservative for v1: we do not iterate to a fixpoint, so
            // back-edge inputs are unknown. A future patch can add widening.
            return Range::top();
        default:
            return Range::top();
        }
    }

    void computeBaseRanges()
    {
        for (BasicBlock* block : m_proc.blocksInPreOrder()) {
            for (Value* v : *block) {
                if (v->type() != Int32)
                    continue;
                m_baseRange[v] = transfer(v);
            }
        }
    }

    // Given that the comparison `lhs OP rhs` is known TRUE under prior ranges
    // `lhsIn` and `rhsIn`, write refined ranges to outLhs/outRhs. Returns true
    // if at least one operand's range is strictly narrower than its input.
    static bool refineForTrueComparison(Opcode op, Range lhsIn, Range rhsIn, Range& outLhs, Range& outRhs)
    {
        outLhs = lhsIn;
        outRhs = rhsIn;
        switch (op) {
        case Equal:
            outLhs = lhsIn.meet(rhsIn);
            outRhs = rhsIn.meet(lhsIn);
            break;
        case NotEqual:
            // Refining an interval against `!= c` is only useful when c is
            // exactly at one endpoint; skip for v1.
            return false;
        case LessThan: // signed
            outLhs.max = std::min(lhsIn.max, rhsIn.max - 1);
            outRhs.min = std::max(rhsIn.min, lhsIn.min + 1);
            break;
        case LessEqual:
            outLhs.max = std::min(lhsIn.max, rhsIn.max);
            outRhs.min = std::max(rhsIn.min, lhsIn.min);
            break;
        case GreaterThan:
            outLhs.min = std::max(lhsIn.min, rhsIn.min + 1);
            outRhs.max = std::min(rhsIn.max, lhsIn.max - 1);
            break;
        case GreaterEqual:
            outLhs.min = std::max(lhsIn.min, rhsIn.min);
            outRhs.max = std::min(rhsIn.max, lhsIn.max);
            break;
        case Below:
        case BelowEqual:
        case Above:
        case AboveEqual:
            // Unsigned: only refine when both inputs are entirely non-negative.
            if (lhsIn.min < 0 || rhsIn.min < 0)
                return false;
            switch (op) {
            case Below:
                outLhs.max = std::min(lhsIn.max, rhsIn.max - 1);
                outRhs.min = std::max(rhsIn.min, lhsIn.min + 1);
                break;
            case BelowEqual:
                outLhs.max = std::min(lhsIn.max, rhsIn.max);
                outRhs.min = std::max(rhsIn.min, lhsIn.min);
                break;
            case Above:
                outLhs.min = std::max(lhsIn.min, rhsIn.min + 1);
                outRhs.max = std::min(rhsIn.max, lhsIn.max - 1);
                break;
            case AboveEqual:
                outLhs.min = std::max(lhsIn.min, rhsIn.min);
                outRhs.max = std::min(rhsIn.max, lhsIn.max);
                break;
            default:
                break;
            }
            break;
        default:
            return false;
        }

        bool narrowed = (outLhs.min > lhsIn.min) || (outLhs.max < lhsIn.max)
            || (outRhs.min > rhsIn.min) || (outRhs.max < rhsIn.max);
        return narrowed;
    }

    void recordRefinement(Value* v, BasicBlock* block, Range refined)
    {
        if (refined.isTop())
            return;
        m_refinements[v].append(Refinement { block, refined });
    }

    void collectRefinementsAt(BasicBlock* successor, Value* cond, bool branchTaken)
    {
        if (successor->numPredecessors() != 1)
            return;

        if (!isComparisonOpcode(cond->opcode()))
            return;

        Value* lhs = cond->child(0);
        Value* rhs = cond->child(1);
        if (lhs->type() != Int32 || rhs->type() != Int32)
            return;

        Opcode effective = branchTaken ? cond->opcode() : invertComparison(cond->opcode());

        Range lhsIn = lookupBase(lhs);
        Range rhsIn = lookupBase(rhs);
        Range lhsOut;
        Range rhsOut;
        if (!refineForTrueComparison(effective, lhsIn, rhsIn, lhsOut, rhsOut))
            return;

        recordRefinement(lhs, successor, lhsOut);
        recordRefinement(rhs, successor, rhsOut);

        dataLogLnIf(B3RangeAnalysisInternal::verbose,
            "Refinement at ", *successor, " (branch ",
            branchTaken ? "taken" : "not-taken", " on ", *cond, "): ",
            *lhs, " -> ", lhsOut, ", ", *rhs, " -> ", rhsOut);
    }

    void collectRefinements()
    {
        for (BasicBlock* P : m_proc) {
            if (P->numSuccessors() != 2)
                continue;
            Value* terminator = P->last();
            if (terminator->opcode() != Branch)
                continue;
            Value* cond = terminator->child(0);
            if (cond->numChildren() != 2)
                continue;

            BasicBlock* takenBlock = P->taken().block();
            BasicBlock* notTakenBlock = P->notTaken().block();
            if (takenBlock == notTakenBlock)
                continue;

            collectRefinementsAt(takenBlock, cond, /* branchTaken */ true);
            collectRefinementsAt(notTakenBlock, cond, /* branchTaken */ false);
        }
    }

    Range rangeAt(BasicBlock* useBlock, Value* v)
    {
        Range r = lookupBase(v);
        const Vector<Refinement>& refs = m_refinements[v];
        if (refs.isEmpty())
            return r;
        Dominators& dominators = m_proc.dominators();
        for (const Refinement& ref : refs) {
            if (dominators.dominates(ref.block, useBlock))
                r = r.meet(ref.range);
        }
        return r;
    }

    static std::optional<int32_t> evaluateSigned(Opcode op, Range lhs, Range rhs)
    {
        switch (op) {
        case Equal:
            if (lhs.max < rhs.min || rhs.max < lhs.min)
                return 0;
            if (lhs.min == lhs.max && rhs.min == rhs.max && lhs.min == rhs.min)
                return 1;
            return std::nullopt;
        case NotEqual:
            if (lhs.max < rhs.min || rhs.max < lhs.min)
                return 1;
            if (lhs.min == lhs.max && rhs.min == rhs.max && lhs.min == rhs.min)
                return 0;
            return std::nullopt;
        case LessThan:
            if (lhs.max < rhs.min)
                return 1;
            if (lhs.min >= rhs.max)
                return 0;
            return std::nullopt;
        case LessEqual:
            if (lhs.max <= rhs.min)
                return 1;
            if (lhs.min > rhs.max)
                return 0;
            return std::nullopt;
        case GreaterThan:
            if (lhs.min > rhs.max)
                return 1;
            if (lhs.max <= rhs.min)
                return 0;
            return std::nullopt;
        case GreaterEqual:
            if (lhs.min >= rhs.max)
                return 1;
            if (lhs.max < rhs.min)
                return 0;
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }

    static std::optional<int32_t> evaluateUnsigned(Opcode op, Range lhs, Range rhs)
    {
        // Only sound when both ranges are entirely in [0, INT32_MAX]; outside
        // that, the unsigned interpretation differs from the signed interval.
        if (lhs.min < 0 || rhs.min < 0)
            return std::nullopt;
        switch (op) {
        case Below:
            if (lhs.max < rhs.min)
                return 1;
            if (lhs.min >= rhs.max)
                return 0;
            return std::nullopt;
        case BelowEqual:
            if (lhs.max <= rhs.min)
                return 1;
            if (lhs.min > rhs.max)
                return 0;
            return std::nullopt;
        case Above:
            if (lhs.min > rhs.max)
                return 1;
            if (lhs.max <= rhs.min)
                return 0;
            return std::nullopt;
        case AboveEqual:
            if (lhs.min >= rhs.max)
                return 1;
            if (lhs.max < rhs.min)
                return 0;
            return std::nullopt;
        default:
            return std::nullopt;
        }
    }

    bool foldComparisons()
    {
        bool changed = false;
        for (BasicBlock* block : m_proc) {
            for (unsigned i = 0; i < block->size(); ++i) {
                Value* v = block->at(i);
                Opcode op = v->opcode();
                if (!isComparisonOpcode(op))
                    continue;
                if (v->numChildren() != 2)
                    continue;
                Value* lhs = v->child(0);
                Value* rhs = v->child(1);
                if (lhs->type() != Int32 || rhs->type() != Int32)
                    continue;

                Range lr = rangeAt(block, lhs);
                Range rr = rangeAt(block, rhs);

                std::optional<int32_t> result;
                switch (op) {
                case Below:
                case BelowEqual:
                case Above:
                case AboveEqual:
                    result = evaluateUnsigned(op, lr, rr);
                    break;
                default:
                    result = evaluateSigned(op, lr, rr);
                    break;
                }
                if (!result)
                    continue;

                dataLogLnIf(B3RangeAnalysisInternal::verbose, "Folding ", *v, " to ", *result, " (lhs=", lr, " rhs=", rr, ")");

                v->replaceWithIdentity(m_insertionSet.insertIntConstant(i, v, *result));
                changed = true;
            }
            m_insertionSet.execute(block);
        }
        return changed;
    }

    struct Refinement {
        BasicBlock* block;
        Range range;
    };

    Procedure& m_proc;
    InsertionSet m_insertionSet;
    IndexMap<Value*, Range> m_baseRange;
    IndexMap<Value*, Vector<Refinement>> m_refinements;
};

} // anonymous namespace

bool rangeAnalysis(Procedure& proc)
{
    PhaseScope phaseScope(proc, "rangeAnalysis"_s);
    RangeAnalysis pass(proc);
    return pass.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
