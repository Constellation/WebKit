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
#include <bit>

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

    static AbstractValue fromFloat(float value)
    {
        AbstractValue result;
        result.m_kind = Kind::Constant;
        result.m_type = Float;
        result.m_double = value;
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

    bool isBottom() const { return m_kind == Kind::Bottom; }
    bool isConstant() const { return m_kind == Kind::Constant; }
    bool isTop() const { return m_kind == Kind::Top; }

    Type type() const { return m_type; }

    int32_t int32Value() const
    {
        ASSERT(isConstant() && m_type == Int32);
        return static_cast<int32_t>(m_int64);
    }

    int64_t int64Value() const
    {
        ASSERT(isConstant() && m_type == Int64);
        return m_int64;
    }

    float floatValue() const
    {
        ASSERT(isConstant() && m_type == Float);
        return static_cast<float>(m_double);
    }

    double doubleValue() const
    {
        ASSERT(isConstant() && m_type == Double);
        return m_double;
    }

    // Merge another abstract value into this one (for phi nodes / join points).
    // Returns true if this value changed.
    bool merge(const AbstractValue& other)
    {
        if (other.isBottom())
            return false; // Merging with Bottom doesn't change anything

        if (isBottom()) {
            *this = other;
            return true;
        }

        if (isTop())
            return false; // Already Top, can't change

        if (other.isTop()) {
            m_kind = Kind::Top;
            return true;
        }

        // Both are constants
        ASSERT(isConstant() && other.isConstant());

        // If types differ, go to Top
        if (m_type != other.m_type) {
            m_kind = Kind::Top;
            return true;
        }

        // Same type, check if values are equal
        bool valuesEqual = false;
        switch (m_type.kind()) {
        case Int32:
            valuesEqual = int32Value() == other.int32Value();
            break;
        case Int64:
            valuesEqual = int64Value() == other.int64Value();
            break;
        case Float:
            valuesEqual = std::bit_cast<uint32_t>(floatValue()) == std::bit_cast<uint32_t>(other.floatValue());
            break;
        case Double:
            valuesEqual = std::bit_cast<uint64_t>(doubleValue()) == std::bit_cast<uint64_t>(other.doubleValue());
            break;
        default:
            valuesEqual = false;
            break;
        }

        if (valuesEqual)
            return false;

        // Different values -> Top
        m_kind = Kind::Top;
        return true;
    }

    explicit operator bool() const
    {
        return !isBottom();
    }

private:
    Kind m_kind;
    Type m_type;
    union {
        int64_t m_int64;
        double m_double;
    };
};

// Pair of Value* and its AbstractValue for sparse per-block storage
struct ValueAbstractValuePair {
    Value* value { nullptr };
    AbstractValue abstractValue;

    ValueAbstractValuePair() = default;
    ValueAbstractValuePair(Value* v, const AbstractValue& av)
        : value(v)
        , abstractValue(av)
    {
    }
};

// Per-block state for SCCP
struct BlockState {
    Vector<ValueAbstractValuePair> valuesAtHead;
    Vector<ValueAbstractValuePair> valuesAtTail;
    bool shouldRevisit { false };
    bool hasVisited { false };

    // Track which edges are executable (for sparse conditional propagation)
    // If empty, all edges are executable (conservative)
    HashSet<BasicBlock*> executableSuccessors;
};

// SCCP pass implementation following DFG's AbstractInterpreter pattern
class SCCP {
public:
    SCCP(Procedure& proc)
        : m_proc(proc)
        , m_blockStates(proc.size())
        , m_abstractValues(proc.values().size())
        , m_phiShadows(proc.values().size())
        , m_insertionSet(proc)
        , m_phiChildren(proc)
    {
        // Initialize block states
        for (BasicBlock* block : proc)
            m_blockStates[block] = BlockState();
    }

    bool run()
    {
        if (B3SCCPInternal::verbose)
            dataLog("B3 SCCP starting\n");

        // Initialize worklist with entry block
        m_worklist.append(m_proc[0]);
        m_blockStates[m_proc[0]].shouldRevisit = true;

        // Fixed-point iteration
        while (!m_worklist.isEmpty()) {
            BasicBlock* block = m_worklist.takeFirst();
            BlockState& blockState = m_blockStates[block];
            blockState.shouldRevisit = false;

            if (B3SCCPInternal::verbose)
                dataLog("Processing block ", *block, "\n");

            beginBasicBlock(block);

            for (Value* value : *block)
                executeValue(value);

            // endBasicBlock merges into successors and adds them to worklist if they changed
            endBasicBlock(block);
        }

        // Apply constant folding transformations
        return applyOptimizations();
    }

private:
    void beginBasicBlock(BasicBlock* block)
    {
        m_block = block;
        BlockState& state = m_blockStates[block];
        state.hasVisited = true;

        // Load valuesAtHead into global state
        for (const ValueAbstractValuePair& pair : state.valuesAtHead) {
            if (pair.value->opcode() == Phi)
                m_phiShadows[pair.value] = pair.abstractValue;
            else
                m_abstractValues[pair.value] = pair.abstractValue;
        }
    }

    bool endBasicBlock(BasicBlock* block)
    {
        BlockState& state = m_blockStates[block];

        // Save current global state to valuesAtTail (sparse - only non-bottom values)
        Vector<ValueAbstractValuePair> newValuesAtTail;

        for (Value* value : m_proc.values()) {
            if (!value)
                continue;

            AbstractValue absValue;
            if (value->opcode() == Phi)
                absValue = m_phiShadows[value];
            else
                absValue = m_abstractValues[value];

            if (absValue)  // Only store non-bottom
                newValuesAtTail.append(ValueAbstractValuePair(value, absValue));
        }

        state.valuesAtTail = WTF::move(newValuesAtTail);

        // Merge into successors' valuesAtHead (sparse conditional: only executable edges)
        bool anySuccessorChanged = false;

        // If executableSuccessors is empty, it means we haven't determined edge executability yet
        // (e.g., block doesn't end with Branch/Switch), so conservatively merge to all
        if (state.executableSuccessors.isEmpty()) {
            for (BasicBlock* successor : block->successorBlocks()) {
                if (mergeIntoSuccessor(block, successor))
                    anySuccessorChanged = true;
            }
        } else {
            // Only merge to executable successors (sparse conditional!)
            for (BasicBlock* successor : state.executableSuccessors) {
                if (mergeIntoSuccessor(block, successor))
                    anySuccessorChanged = true;
            }
        }

        m_block = nullptr;
        return anySuccessorChanged;
    }

    bool mergeIntoSuccessor(BasicBlock* from, BasicBlock* to)
    {
        BlockState& toState = m_blockStates[to];
        BlockState& fromState = m_blockStates[from];

        // For the first visit to 'to', initialize its valuesAtHead from 'from'
        if (toState.valuesAtHead.isEmpty() && !toState.hasVisited) {
            // Copy from's tail to to's head
            for (const ValueAbstractValuePair& pair : fromState.valuesAtTail) {
                toState.valuesAtHead.append(pair);
            }
            if (!toState.shouldRevisit) {
                toState.shouldRevisit = true;
                m_worklist.append(to);
            }
            return true; // Changed
        }

        // Merge: for each value in from's tail, merge into to's head
        bool changed = false;

        // Build a map for quick lookup in toState.valuesAtHead
        HashMap<Value*, unsigned> toHeadIndex;
        for (unsigned i = 0; i < toState.valuesAtHead.size(); ++i)
            toHeadIndex.add(toState.valuesAtHead[i].value, i);

        for (const ValueAbstractValuePair& fromPair : fromState.valuesAtTail) {
            auto it = toHeadIndex.find(fromPair.value);
            if (it != toHeadIndex.end()) {
                // Value exists in to's head, merge
                AbstractValue& toValue = toState.valuesAtHead[it->value].abstractValue;
                if (toValue.merge(fromPair.abstractValue))
                    changed = true;
            } else {
                // New value, add it
                toState.valuesAtHead.append(fromPair);
                changed = true;
            }
        }

        if (changed && !toState.shouldRevisit) {
            toState.shouldRevisit = true;
            m_worklist.append(to);
        }

        return changed;
    }

    bool compareConstants(const AbstractValue& a, const AbstractValue& b)
    {
        if (a.type() != b.type())
            return false;

        switch (a.type().kind()) {
        case Int32:
            return a.int32Value() == b.int32Value();
        case Int64:
            return a.int64Value() == b.int64Value();
        case Float:
            return std::bit_cast<uint32_t>(a.floatValue()) == std::bit_cast<uint32_t>(b.floatValue());
        case Double:
            return std::bit_cast<uint64_t>(a.doubleValue()) == std::bit_cast<uint64_t>(b.doubleValue());
        default:
            return false;
        }
    }

    void executeValue(Value* value)
    {
        if (B3SCCPInternal::verbose)
            dataLog("  Executing ", *value, "\n");

        AbstractValue result = computeAbstractValue(value);

        if (value->opcode() == Upsilon) {
            // Special case: Upsilon updates the phi's shadow
            UpsilonValue* upsilon = value->as<UpsilonValue>();
            Value* phi = upsilon->phi();
            if (phi) {
                AbstractValue phiValue = m_phiShadows[phi];
                if (phiValue.merge(result)) {
                    m_phiShadows[phi] = phiValue;
                }
            }
        } else {
            m_abstractValues[value] = result;

            // Handle control flow for sparse conditional propagation
            switch (value->opcode()) {
            case Branch: {
                // Branch has: child(0) = condition, successors = [taken, notTaken]
                AbstractValue condition = getAbstractValue(value->child(0));
                BlockState& state = m_blockStates[m_block];

                if (condition.isConstant()) {
                    // Known branch direction
                    bool takeBranch = false;
                    if (condition.type() == Int32)
                        takeBranch = condition.int32Value() != 0;
                    else if (condition.type() == Int64)
                        takeBranch = condition.int64Value() != 0;
                    else
                        takeBranch = true; // Conservative for other types

                    if (takeBranch) {
                        // Only taken edge is executable
                        state.executableSuccessors.clear();
                        state.executableSuccessors.add(m_block->taken().block());
                    } else {
                        // Only notTaken edge is executable
                        state.executableSuccessors.clear();
                        state.executableSuccessors.add(m_block->notTaken().block());
                    }
                } else {
                    // Unknown: both edges executable
                    state.executableSuccessors.clear();
                    state.executableSuccessors.add(m_block->taken().block());
                    state.executableSuccessors.add(m_block->notTaken().block());
                }
                break;
            }

            case Switch: {
                // Switch: check if discriminant is constant
                SwitchValue* switchValue = value->as<SwitchValue>();
                AbstractValue discriminant = getAbstractValue(value->child(0));
                BlockState& state = m_blockStates[m_block];
                state.executableSuccessors.clear();

                if (discriminant.isConstant() && discriminant.type() == Int64) {
                    // Known switch value - find matching case
                    int64_t switchVal = discriminant.int64Value();
                    bool foundCase = false;

                    for (const SwitchCase& switchCase : switchValue->cases(m_block)) {
                        if (switchCase.caseValue() == switchVal) {
                            state.executableSuccessors.add(switchCase.targetBlock());
                            foundCase = true;
                            break;
                        }
                    }

                    if (!foundCase) {
                        // Fall through
                        state.executableSuccessors.add(switchValue->fallThrough(m_block).block());
                    }
                } else {
                    // Unknown: all edges executable
                    for (const SwitchCase& switchCase : switchValue->cases(m_block))
                        state.executableSuccessors.add(switchCase.targetBlock());
                    state.executableSuccessors.add(switchValue->fallThrough(m_block).block());
                }
                break;
            }

            default:
                // Other control flow: mark all successors as executable
                if (value->effects().terminal) {
                    BlockState& state = m_blockStates[m_block];
                    state.executableSuccessors.clear();
                    for (BasicBlock* successor : m_block->successorBlocks())
                        state.executableSuccessors.add(successor);
                }
                break;
            }
        }

        if (B3SCCPInternal::verbose)
            dataLog("    Result: ", result.isBottom() ? "Bottom" : result.isConstant() ? "Constant" : "Top", "\n");
    }

    AbstractValue computeAbstractValue(Value* value)
    {
        switch (value->opcode()) {
        case Const32:
            return AbstractValue::fromInt32(value->as<Const32Value>()->value());

        case Const64:
            return AbstractValue::fromInt64(value->as<Const64Value>()->value());

        case ConstFloat:
            return AbstractValue::fromFloat(value->as<ConstFloatValue>()->value());

        case ConstDouble:
            return AbstractValue::fromDouble(value->as<ConstDoubleValue>()->value());

        case Phi: {
            // Phi reads from its shadow
            return m_phiShadows[value];
        }

        case Upsilon: {
            // Upsilon: return the child's value
            return getAbstractValue(value->child(0));
        }

        case Identity:
        case Opaque:
            return getAbstractValue(value->child(0));

        case Add:
        case Sub:
        case Mul:
        case Div:
        case Mod:
            return computeBinaryArithmetic(value);

        case BitAnd:
        case BitOr:
        case BitXor:
        case Shl:
        case SShr:
        case ZShr:
            return computeBitwise(value);

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
        case EqualOrUnordered:
            return computeComparison(value);

        default:
            // Conservative: unknown operations produce Top
            return AbstractValue::top();
        }
    }

    AbstractValue getAbstractValue(Value* value)
    {
        if (value->opcode() == Phi)
            return m_phiShadows[value];
        return m_abstractValues[value];
    }

    AbstractValue computeBinaryArithmetic(Value* value)
    {
        AbstractValue left = getAbstractValue(value->child(0));
        AbstractValue right = getAbstractValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        // TODO: Implement constant folding for various operations
        // For now, be conservative
        return AbstractValue::top();
    }

    AbstractValue computeBitwise(Value* value)
    {
        AbstractValue left = getAbstractValue(value->child(0));
        AbstractValue right = getAbstractValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // TODO: Implement bitwise constant folding
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

        // TODO: Implement comparison constant folding
        return AbstractValue::top();
    }

    bool applyOptimizations()
    {
        // TODO: Replace constant values with actual constants
        // TODO: Remove unreachable blocks
        return false;
    }

    Procedure& m_proc;
    BasicBlock* m_block { nullptr };

    // Per-block state (sparse snapshots)
    IndexMap<BasicBlock*, BlockState> m_blockStates;

    // Global state (current values within a block)
    IndexMap<Value*, AbstractValue> m_abstractValues;
    IndexMap<Value*, AbstractValue> m_phiShadows;

    // Worklist
    Deque<BasicBlock*> m_worklist;

    InsertionSet m_insertionSet;
    PhiChildren m_phiChildren;
};

} // anonymous namespace

bool runSCCP(Procedure& proc)
{
    PhaseScope phaseScope(proc, "B3::runSCCP");

    SCCP sccp(proc);
    return sccp.run();
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
