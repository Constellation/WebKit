/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "B3SCCP.h"

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3CaseCollectionInlines.h"
#include "B3Const32Value.h"
#include "B3Const64Value.h"
#include "B3ConstDoubleValue.h"
#include "B3ConstFloatValue.h"
#include "B3Dominators.h"
#include "B3ExtractValue.h"
#include "B3InsertionSet.h"
#include "B3Opcode.h"
#include "B3PhaseScope.h"
#include "B3PhiChildren.h"
#include "B3Procedure.h"
#include "B3SwitchValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include <wtf/Deque.h>
#include <wtf/IndexMap.h>

namespace JSC { namespace B3 {

namespace {

namespace B3SCCPInternal {
static constexpr bool verbose = false;
}

// AbstractValue represents the abstract state of a B3 value in the SCCP lattice.
// Lattice: Bottom < Constant < Top
//
// Bottom: Value has not been computed yet or is unreachable
// Constant: Value is a known compile-time constant
// Top: Value can be multiple values at runtime
//
// Future extension: Add type-based lattice for WasmGC RTT propagation.
class AbstractValue {
public:
    enum class Kind : uint8_t {
        Bottom,   // Not yet computed / unreachable
        Constant, // Known constant value
        Top       // Unknown / multiple possible values
    };

    AbstractValue()
        : m_kind(Kind::Bottom)
        , m_type(Void)
    {
    }

    static AbstractValue bottom() { return AbstractValue(); }

    static AbstractValue top()
    {
        AbstractValue result;
        result.m_kind = Kind::Top;
        return result;
    }

    static AbstractValue fromInt32(int32_t value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Int32;
        result.m_int64 = value;
        return result;
    }

    static AbstractValue fromInt64(int64_t value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Int64;
        result.m_int64 = value;
        return result;
    }

    static AbstractValue fromDouble(double value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Double;
        result.m_double = value;
        return result;
    }

    static AbstractValue fromFloat(float value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Float;
        result.m_float = value;
        return result;
    }

    static AbstractValue fromValue(Value* value)
    {
        switch (value->opcode()) {
        case Const32:
            return fromInt32(value->as<Const32Value>()->value());
        case Const64:
            return fromInt64(value->as<Const64Value>()->value());
        case ConstDouble:
            return fromDouble(value->as<ConstDoubleValue>()->value());
        case ConstFloat:
            return fromFloat(value->as<ConstFloatValue>()->value());
        default:
            return top();
        }
    }

    bool isBottom() const { return m_kind == Kind::Bottom; }
    bool isTop() const { return m_kind == Kind::Top; }
    bool isConstant() const { return m_kind == Kind::Constant; }

    Type type() const { return m_type; }

    int32_t asInt32() const
    {
        ASSERT(isConstant() && m_type == Int32);
        return static_cast<int32_t>(m_int64);
    }

    int64_t asInt64() const
    {
        ASSERT(isConstant() && m_type == Int64);
        return m_int64;
    }

    double asDouble() const
    {
        ASSERT(isConstant() && m_type == Double);
        return m_double;
    }

    float asFloat() const
    {
        ASSERT(isConstant() && m_type == Float);
        return m_float;
    }

    bool isNonZeroInt() const
    {
        if (!isConstant())
            return false;
        if (m_type == Int32)
            return asInt32() != 0;
        if (m_type == Int64)
            return asInt64() != 0;
        return false;
    }

    bool isZeroInt() const
    {
        if (!isConstant())
            return false;
        if (m_type == Int32)
            return asInt32() == 0;
        if (m_type == Int64)
            return asInt64() == 0;
        return false;
    }

    // Merge another value into this one. Returns true if this value changed.
    // Lattice: Bottom < Constant < Top
    bool merge(const AbstractValue& other)
    {
        if (other.isBottom())
            return false;

        if (isBottom()) {
            *this = other;
            return true;
        }

        if (isTop())
            return false;

        if (other.isTop()) {
            m_kind = Kind::Top;
            return true;
        }

        // Both are constants
        ASSERT(isConstant() && other.isConstant());

        // Check if they're the same constant
        if (m_type != other.m_type) {
            m_kind = Kind::Top;
            return true;
        }

        bool same = false;
        switch (m_type.kind()) {
        case Int32:
            same = asInt32() == other.asInt32();
            break;
        case Int64:
            same = asInt64() == other.asInt64();
            break;
        case Float:
            same = std::bit_cast<uint32_t>(m_float) == std::bit_cast<uint32_t>(other.m_float);
            break;
        case Double:
            same = std::bit_cast<uint64_t>(m_double) == std::bit_cast<uint64_t>(other.m_double);
            break;
        default:
            same = false;
            break;
        }

        if (!same) {
            m_kind = Kind::Top;
            return true;
        }

        return false;
    }

    void dump(PrintStream& out) const
    {
        switch (m_kind) {
        case Kind::Bottom:
            out.print("Bottom"_s);
            break;
        case Kind::Top:
            out.print("Top"_s);
            break;
        case Kind::Constant:
            out.print("Const("_s);
            switch (m_type.kind()) {
            case Int32:
                out.print(asInt32());
                break;
            case Int64:
                out.print(asInt64());
                break;
            case Float:
                out.print(m_float);
                break;
            case Double:
                out.print(m_double);
                break;
            default:
                out.print("?"_s);
                break;
            }
            out.print(")"_s);
            break;
        }
    }

private:
    Kind m_kind { Kind::Bottom };
    Type m_type { Void };
    union {
        int64_t m_int64 { 0 };
        double m_double;
        float m_float;
    };
};

// Key type for sparse maps. We pack (blockIndex, valueIndex) or (tupleValueIndex, elementIndex)
// into a single uint64_t to simplify hashing. We use UnsignedWithZeroKeyHashTraits since
// key 0 is valid (blockIndex=0, valueIndex=0).
using SparseKey = uint64_t;

inline SparseKey makeSparseKey(unsigned high, unsigned low)
{
    return (static_cast<uint64_t>(high) << 32) | low;
}

inline unsigned sparseKeyHigh(SparseKey key)
{
    return static_cast<unsigned>(key >> 32);
}

inline unsigned sparseKeyLow(SparseKey key)
{
    return static_cast<unsigned>(key);
}

class SCCP {
public:
    SCCP(Procedure& proc)
        : m_proc(proc)
        , m_dominators(proc.dominators())
        , m_abstractValues(proc.values().size())
        , m_insertionSet(proc)
        , m_phiChildren(proc)
    {
    }

    bool run()
    {
        if (B3SCCPInternal::verbose)
            dataLog("B3 SCCP starting on:\n"_s, m_proc, "\n"_s);

        // Initialize: Add entry block to worklist
        BasicBlock* entryBlock = m_proc[0];
        m_blockWorklist.append(entryBlock);
        m_blocksOnWorklist.set(entryBlock->index());

        // Fixed-point iteration
        while (!m_blockWorklist.isEmpty()) {
            BasicBlock* block = m_blockWorklist.takeFirst();
            m_blocksOnWorklist.clear(block->index());

            processBlock(block);
        }

        // Apply optimizations
        return applyOptimizations();
    }

private:
    void processBlock(BasicBlock* block)
    {
        if (B3SCCPInternal::verbose)
            dataLog("Processing block "_s, *block, "\n"_s);

        // Check if any predecessor edge is executable
        bool hasExecutablePredecessor = false;
        if (block == m_proc[0]) {
            // Entry block is always executable
            hasExecutablePredecessor = true;
        } else {
            for (BasicBlock* pred : block->predecessors()) {
                if (isEdgeExecutable(pred, block)) {
                    hasExecutablePredecessor = true;
                    break;
                }
            }
        }

        if (!hasExecutablePredecessor)
            return;

        // Process each value in the block
        for (Value* value : *block)
            processValue(block, value);

        // Process terminal and mark successor edges
        processTerminal(block);
    }

    void processValue(BasicBlock* block, Value* value)
    {
        AbstractValue newValue = computeAbstractValue(block, value);

        if (B3SCCPInternal::verbose)
            dataLog("  "_s, *value, " -> "_s, newValue, "\n"_s);

        // Update the abstract value
        // Note: The worklist ensures we revisit blocks when edges become executable.
        // For phi values, PhiChildren handles the upsilon->phi relationship.
        m_abstractValues[value].merge(newValue);
    }

    AbstractValue computeAbstractValue(BasicBlock* block, Value* value)
    {
        // Check for block-specific narrowed value first
        SparseKey narrowKey = makeSparseKey(block->index(), value->index());
        auto it = m_narrowedValues.find(narrowKey);
        if (it != m_narrowedValues.end())
            return it->value;

        switch (value->opcode()) {
        case Const32:
            return AbstractValue::fromInt32(value->as<Const32Value>()->value());

        case Const64:
            return AbstractValue::fromInt64(value->as<Const64Value>()->value());

        case ConstDouble:
            return AbstractValue::fromDouble(value->as<ConstDoubleValue>()->value());

        case ConstFloat:
            return AbstractValue::fromFloat(value->as<ConstFloatValue>()->value());

        case Phi:
            return computePhiValue(block, value);

        case Identity:
        case Opaque:
            return getAbstractValue(value->child(0));

        case Add:
        case Sub:
        case Mul:
        case Div:
        case UDiv:
        case Mod:
        case UMod:
        case BitAnd:
        case BitOr:
        case BitXor:
        case Shl:
        case SShr:
        case ZShr:
            return computeBinaryArithmetic(value);

        case Neg:
        case BitwiseCast:
            return computeUnaryArithmetic(value);

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
            return computeComparison(value);

        case Select:
            return computeSelect(value);

        case Extract:
            return computeExtract(value);

        default:
            // For unknown opcodes, return Top (unknown)
            return AbstractValue::top();
        }
    }

    AbstractValue computePhiValue(BasicBlock* block, Value* phi)
    {
        AbstractValue result = AbstractValue::bottom();

        for (UpsilonValue* upsilon : m_phiChildren[phi]) {
            BasicBlock* upsilonBlock = upsilon->owner;
            if (!upsilonBlock)
                continue;

            // An upsilon contributes to a phi if there's an executable path from the upsilon
            // to the phi. We check this by seeing if:
            // 1. The upsilon's block dominates a predecessor of the phi's block, AND
            // 2. That predecessor→phi edge is executable
            // OR
            // 3. The upsilon's block IS a predecessor and that edge is executable (for simple cases)
            bool upsilonCanReachPhi = false;

            for (BasicBlock* pred : block->predecessors()) {
                // Case 1: Direct predecessor
                if (pred == upsilonBlock && isEdgeExecutable(pred, block)) {
                    upsilonCanReachPhi = true;
                    break;
                }

                // Case 2: Upsilon block dominates the predecessor
                if (m_dominators.dominates(upsilonBlock, pred) && isEdgeExecutable(pred, block)) {
                    upsilonCanReachPhi = true;
                    break;
                }
            }

            if (!upsilonCanReachPhi)
                continue;

            AbstractValue childValue = getAbstractValue(upsilon->child(0));
            result.merge(childValue);

            // Early exit if we hit Top
            if (result.isTop())
                break;
        }

        return result;
    }

    AbstractValue computeBinaryArithmetic(Value* value)
    {
        AbstractValue left = getAbstractValue(value->child(0));
        AbstractValue right = getAbstractValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // Both are constants - try to fold
        ASSERT(left.isConstant() && right.isConstant());

        // Only fold integer arithmetic for now
        if (left.type() != right.type())
            return AbstractValue::top();

        Type type = left.type();

        if (type == Int32) {
            int32_t l = left.asInt32();
            int32_t r = right.asInt32();
            int32_t result;

            switch (value->opcode()) {
            case Add:
                result = l + r;
                break;
            case Sub:
                result = l - r;
                break;
            case Mul:
                result = l * r;
                break;
            case Div:
                if (r == 0)
                    return AbstractValue::top();
                result = l / r;
                break;
            case UDiv:
                if (r == 0)
                    return AbstractValue::top();
                result = static_cast<int32_t>(static_cast<uint32_t>(l) / static_cast<uint32_t>(r));
                break;
            case Mod:
                if (r == 0)
                    return AbstractValue::top();
                result = l % r;
                break;
            case UMod:
                if (r == 0)
                    return AbstractValue::top();
                result = static_cast<int32_t>(static_cast<uint32_t>(l) % static_cast<uint32_t>(r));
                break;
            case BitAnd:
                result = l & r;
                break;
            case BitOr:
                result = l | r;
                break;
            case BitXor:
                result = l ^ r;
                break;
            case Shl:
                result = l << (r & 31);
                break;
            case SShr:
                result = l >> (r & 31);
                break;
            case ZShr:
                result = static_cast<int32_t>(static_cast<uint32_t>(l) >> (r & 31));
                break;
            default:
                return AbstractValue::top();
            }

            return AbstractValue::fromInt32(result);
        }

        if (type == Int64) {
            int64_t l = left.asInt64();
            int64_t r = right.asInt64();
            int64_t result;

            switch (value->opcode()) {
            case Add:
                result = l + r;
                break;
            case Sub:
                result = l - r;
                break;
            case Mul:
                result = l * r;
                break;
            case Div:
                if (r == 0)
                    return AbstractValue::top();
                result = l / r;
                break;
            case UDiv:
                if (r == 0)
                    return AbstractValue::top();
                result = static_cast<int64_t>(static_cast<uint64_t>(l) / static_cast<uint64_t>(r));
                break;
            case Mod:
                if (r == 0)
                    return AbstractValue::top();
                result = l % r;
                break;
            case UMod:
                if (r == 0)
                    return AbstractValue::top();
                result = static_cast<int64_t>(static_cast<uint64_t>(l) % static_cast<uint64_t>(r));
                break;
            case BitAnd:
                result = l & r;
                break;
            case BitOr:
                result = l | r;
                break;
            case BitXor:
                result = l ^ r;
                break;
            case Shl:
                result = l << (r & 63);
                break;
            case SShr:
                result = l >> (r & 63);
                break;
            case ZShr:
                result = static_cast<int64_t>(static_cast<uint64_t>(l) >> (r & 63));
                break;
            default:
                return AbstractValue::top();
            }

            return AbstractValue::fromInt64(result);
        }

        return AbstractValue::top();
    }

    AbstractValue computeUnaryArithmetic(Value* value)
    {
        AbstractValue child = getAbstractValue(value->child(0));

        if (child.isBottom())
            return AbstractValue::bottom();

        if (child.isTop())
            return AbstractValue::top();

        ASSERT(child.isConstant());

        switch (value->opcode()) {
        case Neg:
            if (child.type() == Int32)
                return AbstractValue::fromInt32(-child.asInt32());
            if (child.type() == Int64)
                return AbstractValue::fromInt64(-child.asInt64());
            if (child.type() == Float)
                return AbstractValue::fromFloat(-child.asFloat());
            if (child.type() == Double)
                return AbstractValue::fromDouble(-child.asDouble());
            break;

        case BitwiseCast:
            if (child.type() == Int32 && value->type() == Float)
                return AbstractValue::fromFloat(std::bit_cast<float>(child.asInt32()));
            if (child.type() == Float && value->type() == Int32)
                return AbstractValue::fromInt32(std::bit_cast<int32_t>(child.asFloat()));
            if (child.type() == Int64 && value->type() == Double)
                return AbstractValue::fromDouble(std::bit_cast<double>(child.asInt64()));
            if (child.type() == Double && value->type() == Int64)
                return AbstractValue::fromInt64(std::bit_cast<int64_t>(child.asDouble()));
            break;

        default:
            break;
        }

        return AbstractValue::top();
    }

    AbstractValue computeComparison(Value* value)
    {
        AbstractValue left = getAbstractValue(value->child(0));
        AbstractValue right = getAbstractValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        ASSERT(left.isConstant() && right.isConstant());

        if (left.type() != right.type())
            return AbstractValue::top();

        Type type = left.type();

        if (type == Int32) {
            int32_t l = left.asInt32();
            int32_t r = right.asInt32();
            int32_t result;

            switch (value->opcode()) {
            case Equal:
                result = l == r ? 1 : 0;
                break;
            case NotEqual:
                result = l != r ? 1 : 0;
                break;
            case LessThan:
                result = l < r ? 1 : 0;
                break;
            case GreaterThan:
                result = l > r ? 1 : 0;
                break;
            case LessEqual:
                result = l <= r ? 1 : 0;
                break;
            case GreaterEqual:
                result = l >= r ? 1 : 0;
                break;
            case Above:
                result = static_cast<uint32_t>(l) > static_cast<uint32_t>(r) ? 1 : 0;
                break;
            case Below:
                result = static_cast<uint32_t>(l) < static_cast<uint32_t>(r) ? 1 : 0;
                break;
            case AboveEqual:
                result = static_cast<uint32_t>(l) >= static_cast<uint32_t>(r) ? 1 : 0;
                break;
            case BelowEqual:
                result = static_cast<uint32_t>(l) <= static_cast<uint32_t>(r) ? 1 : 0;
                break;
            default:
                return AbstractValue::top();
            }

            return AbstractValue::fromInt32(result);
        }

        if (type == Int64) {
            int64_t l = left.asInt64();
            int64_t r = right.asInt64();
            int32_t result;

            switch (value->opcode()) {
            case Equal:
                result = l == r ? 1 : 0;
                break;
            case NotEqual:
                result = l != r ? 1 : 0;
                break;
            case LessThan:
                result = l < r ? 1 : 0;
                break;
            case GreaterThan:
                result = l > r ? 1 : 0;
                break;
            case LessEqual:
                result = l <= r ? 1 : 0;
                break;
            case GreaterEqual:
                result = l >= r ? 1 : 0;
                break;
            case Above:
                result = static_cast<uint64_t>(l) > static_cast<uint64_t>(r) ? 1 : 0;
                break;
            case Below:
                result = static_cast<uint64_t>(l) < static_cast<uint64_t>(r) ? 1 : 0;
                break;
            case AboveEqual:
                result = static_cast<uint64_t>(l) >= static_cast<uint64_t>(r) ? 1 : 0;
                break;
            case BelowEqual:
                result = static_cast<uint64_t>(l) <= static_cast<uint64_t>(r) ? 1 : 0;
                break;
            default:
                return AbstractValue::top();
            }

            return AbstractValue::fromInt32(result);
        }

        return AbstractValue::top();
    }

    AbstractValue computeSelect(Value* value)
    {
        AbstractValue condition = getAbstractValue(value->child(0));

        if (condition.isBottom())
            return AbstractValue::bottom();

        if (condition.isConstant()) {
            bool cond = false;
            if (condition.type() == Int32)
                cond = condition.asInt32() != 0;
            else if (condition.type() == Int64)
                cond = condition.asInt64() != 0;
            else
                return AbstractValue::top();

            return getAbstractValue(cond ? value->child(1) : value->child(2));
        }

        // Condition is Top - merge both branches
        AbstractValue thenValue = getAbstractValue(value->child(1));
        AbstractValue elseValue = getAbstractValue(value->child(2));
        thenValue.merge(elseValue);
        return thenValue;
    }

    AbstractValue computeExtract(Value* value)
    {
        Value* tuple = value->child(0);
        int32_t index = value->as<ExtractValue>()->index();

        SparseKey key = makeSparseKey(tuple->index(), index);
        auto it = m_tupleElements.find(key);
        if (it != m_tupleElements.end())
            return it->value;

        return AbstractValue::top();
    }

    void processTerminal(BasicBlock* block)
    {
        Value* terminal = block->last();

        switch (terminal->opcode()) {
        case Branch: {
            AbstractValue condition = getAbstractValue(terminal->child(0));
            BasicBlock* taken = block->successorBlock(0);
            BasicBlock* notTaken = block->successorBlock(1);

            // Path-sensitive: record that the condition is non-zero on taken path
            // and zero on not-taken path (like foldPathConstants)
            if (taken != notTaken) {
                if (taken->numPredecessors() == 1) {
                    SparseKey takenKey = makeSparseKey(taken->index(), terminal->child(0)->index());
                    // Mark as non-zero (we use Top with isNonZero info in narrowedValues)
                    m_isNonZeroOnPath.add(takenKey);
                }
                if (notTaken->numPredecessors() == 1 && !condition.isConstant()) {
                    // On not-taken path, condition is known to be zero
                    SparseKey notTakenKey = makeSparseKey(notTaken->index(), terminal->child(0)->index());
                    if (terminal->child(0)->type() == Int32)
                        m_narrowedValues.set(notTakenKey, AbstractValue::fromInt32(0));
                    else if (terminal->child(0)->type() == Int64)
                        m_narrowedValues.set(notTakenKey, AbstractValue::fromInt64(0));
                }
            }

            if (condition.isBottom()) {
                // Not yet computable
                return;
            }

            if (condition.isConstant()) {
                bool cond = false;
                if (condition.type() == Int32)
                    cond = condition.asInt32() != 0;
                else if (condition.type() == Int64)
                    cond = condition.asInt64() != 0;

                // Only one successor is executable
                BasicBlock* successor = cond ? taken : notTaken;
                markEdgeExecutable(block, successor);
            } else {
                // Both successors are executable
                markEdgeExecutable(block, taken);
                markEdgeExecutable(block, notTaken);
            }
            break;
        }

        case Switch: {
            SwitchValue* switchValue = terminal->as<SwitchValue>();
            AbstractValue condition = getAbstractValue(terminal->child(0));

            // Path-sensitive: record exact values for switch cases (like foldPathConstants)
            UncheckedKeyHashMap<BasicBlock*, unsigned> targetUses;
            for (SwitchCase switchCase : switchValue->cases(block))
                targetUses.add(switchCase.targetBlock(), 0).iterator->value++;
            targetUses.add(switchValue->fallThrough(block), 0).iterator->value++;

            for (SwitchCase switchCase : switchValue->cases(block)) {
                if (targetUses.find(switchCase.targetBlock())->value != 1)
                    continue;

                BasicBlock* caseBlock = switchCase.targetBlock();
                if (caseBlock->numPredecessors() == 1) {
                    SparseKey key = makeSparseKey(caseBlock->index(), terminal->child(0)->index());
                    if (terminal->child(0)->type() == Int32)
                        m_narrowedValues.set(key, AbstractValue::fromInt32(static_cast<int32_t>(switchCase.caseValue())));
                    else if (terminal->child(0)->type() == Int64)
                        m_narrowedValues.set(key, AbstractValue::fromInt64(switchCase.caseValue()));
                }
            }

            if (condition.isBottom())
                return;

            if (condition.isConstant()) {
                int64_t caseValue = 0;
                if (condition.type() == Int32)
                    caseValue = condition.asInt32();
                else if (condition.type() == Int64)
                    caseValue = condition.asInt64();
                else {
                    // Unknown constant type - mark all edges executable
                    for (BasicBlock* successor : block->successorBlocks())
                        markEdgeExecutable(block, successor);
                    return;
                }

                // Find the matching case
                bool found = false;
                for (SwitchCase switchCase : switchValue->cases(block)) {
                    if (switchCase.caseValue() == caseValue) {
                        markEdgeExecutable(block, switchCase.targetBlock());
                        found = true;
                        break;
                    }
                }

                if (!found)
                    markEdgeExecutable(block, switchValue->fallThrough(block));
            } else {
                // All successors are executable
                for (BasicBlock* successor : block->successorBlocks())
                    markEdgeExecutable(block, successor);
            }
            break;
        }

        case Jump:
            markEdgeExecutable(block, block->successorBlock(0));
            break;

        case Return:
        case Oops:
            // No successors
            break;

        default:
            // For unknown terminals, mark all successors executable
            for (BasicBlock* successor : block->successorBlocks())
                markEdgeExecutable(block, successor);
            break;
        }
    }

    AbstractValue getAbstractValue(Value* value)
    {
        return m_abstractValues[value];
    }

    bool isEdgeExecutable(BasicBlock* from, BasicBlock* to)
    {
        SparseKey key = makeSparseKey(from->index(), to->index());
        return m_executableEdges.contains(key);
    }

    void markEdgeExecutable(BasicBlock* from, BasicBlock* to)
    {
        SparseKey key = makeSparseKey(from->index(), to->index());
        if (m_executableEdges.add(key).isNewEntry) {
            // Edge became executable - add target to worklist
            if (!m_blocksOnWorklist.get(to->index())) {
                m_blockWorklist.append(to);
                m_blocksOnWorklist.set(to->index());
            }
        }
    }

    bool applyOptimizations()
    {
        bool changed = false;

        for (BasicBlock* block : m_proc) {
            // Skip blocks that are unreachable
            if (block != m_proc[0]) {
                bool anyExecutablePred = false;
                for (BasicBlock* pred : block->predecessors()) {
                    if (isEdgeExecutable(pred, block)) {
                        anyExecutablePred = true;
                        break;
                    }
                }
                if (!anyExecutablePred)
                    continue;
            }

            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);

                // Handle path-sensitive optimizations (like foldPathConstants)
                switch (value->opcode()) {
                case Branch: {
                    SparseKey key = makeSparseKey(block->index(), value->child(0)->index());
                    if (m_isNonZeroOnPath.contains(key)) {
                        // We know condition is non-zero at this point
                        // This is for when we're in a block dominated by a taken branch
                    }

                    // Check if condition is a constant
                    AbstractValue cond = getAbstractValue(value->child(0));
                    if (cond.isNonZeroInt()) {
                        value->replaceWithJump(block, block->taken());
                        changed = true;
                    } else if (cond.isZeroInt()) {
                        value->replaceWithJump(block, block->notTaken());
                        changed = true;
                    }
                    break;
                }

                case Equal: {
                    if (value->child(1)->isInt(0)) {
                        SparseKey key = makeSparseKey(block->index(), value->child(0)->index());
                        if (m_isNonZeroOnPath.contains(key)) {
                            // x == 0 when we know x is non-zero -> false
                            value->replaceWithIdentity(
                                m_insertionSet.insertIntConstant(valueIndex, value, 0));
                            changed = true;
                            continue;
                        }
                    }
                    break;
                }

                case NotEqual: {
                    if (value->child(1)->isInt(0)) {
                        SparseKey key = makeSparseKey(block->index(), value->child(0)->index());
                        if (m_isNonZeroOnPath.contains(key)) {
                            // x != 0 when we know x is non-zero -> true
                            value->replaceWithIdentity(
                                m_insertionSet.insertIntConstant(valueIndex, value, 1));
                            changed = true;
                            continue;
                        }
                    }
                    break;
                }

                default:
                    break;
                }

                // Replace with constant if we know the value
                AbstractValue abstractValue = getAbstractValue(value);

                if (abstractValue.isConstant() && !value->isConstant()) {
                    Value* constValue = nullptr;

                    switch (abstractValue.type().kind()) {
                    case Int32:
                        constValue = m_insertionSet.insert<Const32Value>(
                            valueIndex, value->origin(), abstractValue.asInt32());
                        break;
                    case Int64:
                        constValue = m_insertionSet.insert<Const64Value>(
                            valueIndex, value->origin(), abstractValue.asInt64());
                        break;
                    case Float:
                        constValue = m_insertionSet.insert<ConstFloatValue>(
                            valueIndex, value->origin(), abstractValue.asFloat());
                        break;
                    case Double:
                        constValue = m_insertionSet.insert<ConstDoubleValue>(
                            valueIndex, value->origin(), abstractValue.asDouble());
                        break;
                    default:
                        break;
                    }

                    if (constValue) {
                        value->replaceWithIdentity(constValue);
                        changed = true;
                    }
                }

                // Replace uses with path-narrowed constants
                for (Value*& child : value->children()) {
                    SparseKey key = makeSparseKey(block->index(), child->index());
                    auto it = m_narrowedValues.find(key);
                    if (it != m_narrowedValues.end() && it->value.isConstant()) {
                        Value* constValue = nullptr;
                        AbstractValue narrowed = it->value;

                        switch (narrowed.type().kind()) {
                        case Int32:
                            constValue = m_insertionSet.insert<Const32Value>(
                                valueIndex, child->origin(), narrowed.asInt32());
                            break;
                        case Int64:
                            constValue = m_insertionSet.insert<Const64Value>(
                                valueIndex, child->origin(), narrowed.asInt64());
                            break;
                        default:
                            break;
                        }

                        if (constValue) {
                            child = constValue;
                            changed = true;
                        }
                    }
                }
            }

            m_insertionSet.execute(block);
        }

        if (changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }

        return changed;
    }

    Procedure& m_proc;
    Dominators& m_dominators;

    // Global abstract values for each Value*
    IndexMap<Value*, AbstractValue> m_abstractValues;

    // Sparse per-block narrowed values (for path-sensitive type refinement)
    // Key: makeSparseKey(blockIndex, valueIndex)
    HashMap<SparseKey, AbstractValue, IntHash<SparseKey>, WTF::UnsignedWithZeroKeyHashTraits<SparseKey>> m_narrowedValues;

    // Track which values are known non-zero on certain paths
    HashSet<SparseKey, IntHash<SparseKey>, WTF::UnsignedWithZeroKeyHashTraits<SparseKey>> m_isNonZeroOnPath;

    // Tuple element abstract values
    // Key: makeSparseKey(tupleValueIndex, elementIndex)
    HashMap<SparseKey, AbstractValue, IntHash<SparseKey>, WTF::UnsignedWithZeroKeyHashTraits<SparseKey>> m_tupleElements;

    // Executable edges
    // Key: makeSparseKey(fromBlockIndex, toBlockIndex)
    HashSet<SparseKey, IntHash<SparseKey>, WTF::UnsignedWithZeroKeyHashTraits<SparseKey>> m_executableEdges;

    // Block worklist for fixed-point iteration
    Deque<BasicBlock*> m_blockWorklist;
    BitVector m_blocksOnWorklist;

    InsertionSet m_insertionSet;
    PhiChildren m_phiChildren;
};

} // anonymous namespace

bool runSCCP(Procedure& proc)
{
    PhaseScope phaseScope(proc, "runSCCP"_s);
    SCCP sccp(proc);
    return sccp.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
