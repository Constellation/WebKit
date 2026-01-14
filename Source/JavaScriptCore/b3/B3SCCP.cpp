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
        result.m_int32 = value;
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
        result.m_float = value;
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
        return m_int32;
    }

    int64_t int64Value() const
    {
        ASSERT(isConstant() && m_type == Int64);
        return m_int64;
    }

    float floatValue() const
    {
        ASSERT(isConstant() && m_type == Float);
        return m_float;
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
        int32_t m_int32;
        int64_t m_int64;
        float m_float;
        double m_double;
    };
};

} // anonymous namespace

// ValueFlowProjection - encodes Value* and whether it's Shadow (for Phi) or Primary
// Must be outside anonymous namespace for WTF hash traits
// (matching DFG's NodeFlowProjection)
class ValueFlowProjection {
public:
    enum Kind {
        Primary,
        Shadow
    };

    ValueFlowProjection() = default;

    ValueFlowProjection(Value* value)
        : m_word(std::bit_cast<uintptr_t>(value))
    {
        ASSERT(kind() == Primary);
    }

    ValueFlowProjection(Value* value, Kind kind)
        : m_word(std::bit_cast<uintptr_t>(value) | (kind == Shadow ? shadowBit : 0))
    {
        ASSERT(this->kind() == kind);
    }

    ValueFlowProjection(WTF::HashTableDeletedValueType)
        : m_word(shadowBit)
    {
    }

    explicit operator bool() const { return !!m_word; }

    Kind kind() const { return (m_word & shadowBit) ? Shadow : Primary; }

    Value* value() const { return std::bit_cast<Value*>(m_word & ~shadowBit); }

    Value& operator*() const { return *value(); }
    Value* operator->() const { return value(); }

    unsigned hash() const
    {
        return m_word;
    }

    friend bool operator==(const ValueFlowProjection&, const ValueFlowProjection&) = default;

    bool operator<(ValueFlowProjection other) const
    {
        if (kind() != other.kind())
            return kind() < other.kind();
        return value() < other.value();
    }

    bool operator>(ValueFlowProjection other) const
    {
        return other < *this;
    }

    bool operator<=(ValueFlowProjection other) const
    {
        return !(*this > other);
    }

    bool operator>=(ValueFlowProjection other) const
    {
        return !(*this < other);
    }

    bool isHashTableDeletedValue() const
    {
        return *this == ValueFlowProjection(WTF::HashTableDeletedValue);
    }

    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    // Phi shadow projections can become invalid because the Phi might be folded to something else.
    bool isStillValid() const
    {
        return *this && (kind() == Primary || value()->opcode() == Phi);
    }

    template<typename Func>
    static void forEach(Value* value, const Func& func)
    {
        func(ValueFlowProjection(value));
        if (value->opcode() == Phi)
            func(ValueFlowProjection(value, Shadow));
    }

private:
    static constexpr uintptr_t shadowBit = 1;
    uintptr_t m_word { 0 };
};

} } // namespace JSC::B3

// WTF hash support for ValueFlowProjection (matching DFG's NodeFlowProjection)
namespace WTF {

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::B3::ValueFlowProjection> : SimpleClassHashTraits<JSC::B3::ValueFlowProjection> { };

} // namespace WTF

namespace JSC { namespace B3 {

namespace {

// Pair of ValueFlowProjection and its AbstractValue (matching DFG's NodeAbstractValuePair)
struct ValueAbstractValuePair {
    ValueFlowProjection value;
    AbstractValue abstractValue;

    ValueAbstractValuePair() = default;
    ValueAbstractValuePair(ValueFlowProjection v, const AbstractValue& av)
        : value(v)
        , abstractValue(av)
    {
    }
};

// FlowMap for B3 (matching DFG's FlowMap)
// Maps Value indices to AbstractValues, with separate storage for Phi shadows
template<typename T>
class FlowMap {
public:
    FlowMap(Procedure& proc)
        : m_proc(proc)
        , m_map(proc.values().size())
        , m_shadowMap(proc.values().size())
    {
    }

    void resize()
    {
        m_map.resize(m_proc.values().size());
        m_shadowMap.resize(m_proc.values().size());
    }

    // Access using ValueFlowProjection
    T& at(ValueFlowProjection projection)
    {
        if (projection.kind() == ValueFlowProjection::Shadow)
            return m_shadowMap[projection.value()];
        return m_map[projection.value()];
    }

private:
    Procedure& m_proc;
    IndexMap<Value*, T> m_map;
    IndexMap<Value*, T> m_shadowMap;
};

// Per-block state for SCCP (matching DFG's BasicBlock::SSAData)
struct BlockState {
    Vector<ValueFlowProjection> liveAtHead;  // Live projections at block entry
    Vector<ValueFlowProjection> liveAtTail;  // Live projections at block exit
    Vector<ValueAbstractValuePair> valuesAtHead;  // Pre-populated from liveAtHead
    Vector<ValueAbstractValuePair> valuesAtTail;  // Pre-populated from liveAtTail
    bool shouldRevisit { false };
    bool hasVisited { false };
};

// SCCP pass implementation following DFG's AbstractInterpreter pattern
class SCCP {
public:
    SCCP(Procedure& proc)
        : m_proc(proc)
        , m_abstractValues(proc)
        , m_blockStates(proc.size())
        , m_insertionSet(proc)
        , m_phiChildren(proc)
    {
        // Compute liveness and initialize block states (matching DFG's initialize())
        computeLiveness();

        // Pre-populate valuesAtHead and valuesAtTail from liveness
        // (matching DFG's setLiveValues at line 191-192)
        for (BasicBlock* block : proc) {
            BlockState& state = m_blockStates[block];

            state.valuesAtHead = state.liveAtHead.map([](ValueFlowProjection projection) {
                return ValueAbstractValuePair(projection, AbstractValue());
            });

            state.valuesAtTail = state.liveAtTail.map([](ValueFlowProjection projection) {
                return ValueAbstractValuePair(projection, AbstractValue());
            });
        }
    }

    void computeLiveness()
    {
        // Backward dataflow liveness analysis tracking ValueFlowProjections
        // (matching DFG's LivenessAnalysisPhase lines 120-145)
        // Live = projections that are used in this block or live in successors

        // Iterate to fixpoint
        bool changed = true;
        while (changed) {
            changed = false;

            // Process blocks in reverse order (backward analysis)
            for (unsigned i = m_proc.size(); i--;) {
                BasicBlock* block = m_proc[i];
                if (!block)
                    continue;

                BlockState& state = m_blockStates[block];

                // Start with live-out from successors (liveAtHead of successors)
                HashSet<ValueFlowProjection> live;
                for (BasicBlock* successor : block->successorBlocks()) {
                    BlockState& succState = m_blockStates[successor];
                    for (ValueFlowProjection projection : succState.liveAtHead)
                        live.add(projection);
                }

                // Process values in reverse order
                for (unsigned j = block->size(); j--;) {
                    Value* value = block->at(j);

                    // Special handling for Upsilon and Phi (matching DFG lines 127-139)
                    switch (value->opcode()) {
                    case Upsilon: {
                        // Upsilon defines (kills) the Phi's shadow, uses its child
                        UpsilonValue* upsilon = value->as<UpsilonValue>();
                        Value* phi = upsilon->phi();
                        if (phi) {
                            live.remove(ValueFlowProjection(phi, ValueFlowProjection::Shadow));
                            live.add(ValueFlowProjection(upsilon->child(0)));
                        }
                        break;
                    }

                    case Phi: {
                        // Phi defines (kills) its primary, uses (requires live) its shadow
                        live.remove(ValueFlowProjection(value, ValueFlowProjection::Primary));
                        live.add(ValueFlowProjection(value, ValueFlowProjection::Shadow));
                        break;
                    }

                    default:
                        // Regular value: kills its primary, uses its children
                        live.remove(ValueFlowProjection(value, ValueFlowProjection::Primary));
                        for (Value* child : value->children())
                            live.add(ValueFlowProjection(child, ValueFlowProjection::Primary));
                        break;
                    }
                }

                // Convert live set to vector for liveAtHead
                Vector<ValueFlowProjection> newLiveHead;
                for (ValueFlowProjection projection : live)
                    newLiveHead.append(projection);

                // liveAtTail is union of successors' liveAtHead
                Vector<ValueFlowProjection> newLiveTail;
                for (BasicBlock* successor : block->successorBlocks()) {
                    BlockState& succState = m_blockStates[successor];
                    for (ValueFlowProjection projection : succState.liveAtHead)
                        newLiveTail.append(projection);
                }

                if (newLiveHead != state.liveAtHead) {
                    state.liveAtHead = ::WTF::move(newLiveHead);
                    changed = true;
                }
                if (newLiveTail != state.liveAtTail) {
                    state.liveAtTail = ::WTF::move(newLiveTail);
                    changed = true;
                }
            }
        }
    }

    bool run()
    {
        dataLogLnIf(B3SCCPInternal::verbose, "B3 SCCP starting");

        // Initialize worklist with entry block
        m_worklist.append(m_proc[0]);
        m_blockStates[m_proc[0]].shouldRevisit = true;

        // Fixed-point iteration
        while (!m_worklist.isEmpty()) {
            BasicBlock* block = m_worklist.takeFirst();
            BlockState& blockState = m_blockStates[block];
            blockState.shouldRevisit = false;

            dataLogLnIf(B3SCCPInternal::verbose, "Processing block ", *block);

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

        // Load valuesAtHead into global state (matching DFG lines 78-84)
        for (ValueAbstractValuePair& entry : state.valuesAtHead) {
            if (entry.value.isStillValid())
                m_abstractValues.at(entry.value) = entry.abstractValue;
        }
    }

    void endBasicBlock(BasicBlock* block)
    {
        BlockState& state = m_blockStates[block];

        // Save current global state to valuesAtTail (matching DFG lines 307-315)
        for (ValueAbstractValuePair& entry : state.valuesAtTail)
            entry.abstractValue = m_abstractValues.at(entry.value);

        // DON'T clear global m_abstractValues!
        // It persists and will be read by merge()
        mergeToSuccessors(block);

        // Reset block-local state only (matching DFG's reset() at line 328-334)
        m_block = nullptr;
    }

    bool mergeToSuccessors(BasicBlock* block)
    {
        Value* terminal = block->last();
        if (!terminal)
            return false;

        bool changed = false;

        switch (terminal->opcode()) {
        case Jump:
        case Oops:
        case Return:
            // Unconditional: merge to all successors
            for (BasicBlock* successor : block->successorBlocks()) {
                if (mergeIntoSuccessor(block, successor))
                    changed = true;
            }
            break;

        case Branch: {
            // Sparse conditional: check if we know the branch direction
            AbstractValue condition = forValue(terminal->child(0));

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
                    if (mergeIntoSuccessor(block, block->taken().block()))
                        changed = true;
                } else {
                    // Only notTaken edge is executable
                    if (mergeIntoSuccessor(block, block->notTaken().block()))
                        changed = true;
                }
            } else {
                // Unknown: both edges executable
                if (mergeIntoSuccessor(block, block->taken().block()))
                    changed = true;
                if (mergeIntoSuccessor(block, block->notTaken().block()))
                    changed = true;
            }
            break;
        }

        case Switch: {
            // Check if discriminant is constant
            SwitchValue* switchValue = terminal->as<SwitchValue>();
            AbstractValue discriminant = forValue(terminal->child(0));

            if (discriminant.isConstant() && discriminant.type() == Int64) {
                // Known switch value - find matching case
                int64_t switchVal = discriminant.int64Value();
                bool foundCase = false;

                for (SwitchCase switchCase : switchValue->cases(block)) {
                    if (switchCase.caseValue() == switchVal) {
                        if (mergeIntoSuccessor(block, switchCase.targetBlock()))
                            changed = true;
                        foundCase = true;
                        break;
                    }
                }

                if (!foundCase) {
                    // Fall through
                    if (mergeIntoSuccessor(block, switchValue->fallThrough(block)))
                        changed = true;
                }
            } else {
                // Unknown: all edges executable
                for (SwitchCase switchCase : switchValue->cases(block)) {
                    if (mergeIntoSuccessor(block, switchCase.targetBlock()))
                        changed = true;
                }
                if (mergeIntoSuccessor(block, switchValue->fallThrough(block)))
                    changed = true;
            }
            break;
        }

        default:
            // Other control flow: conservatively merge to all successors
            for (BasicBlock* successor : block->successorBlocks()) {
                if (mergeIntoSuccessor(block, successor))
                    changed = true;
            }
            break;
        }

        return changed;
    }

    bool mergeIntoSuccessor(BasicBlock*, BasicBlock* to)
    {
        BlockState& toState = m_blockStates[to];

        bool changed = false;

        // Merge from global m_abstractValues into successor's valuesAtHead
        // (matching DFG lines 370-391)
        for (ValueAbstractValuePair& entry : toState.valuesAtHead) {
            // Read from global FlowMap (which has predecessor's values)
            AbstractValue fromValue = m_abstractValues.at(entry.value);
            // Merge into successor's valuesAtHead
            if (entry.abstractValue.merge(fromValue))
                changed = true;
        }

        if (changed && !toState.shouldRevisit) {
            toState.shouldRevisit = true;
            m_worklist.append(to);
        }

        return changed;
    }

    void executeValue(Value* value)
    {
        dataLogLnIf(B3SCCPInternal::verbose, "  Executing ", *value);

        AbstractValue result = computeAbstractValue(value);

        if (value->opcode() == Upsilon) {
            // Special case: Upsilon updates the phi's shadow projection
            UpsilonValue* upsilon = value->as<UpsilonValue>();
            Value* phi = upsilon->phi();
            if (phi) {
                AbstractValue& phiValue = forValue(ValueFlowProjection(phi, ValueFlowProjection::Shadow));
                phiValue.merge(result);
            }
        } else {
            forValue(value) = result;
        }

        dataLogLnIf(B3SCCPInternal::verbose, "    Result: ", result.isBottom() ? "Bottom" : result.isConstant() ? "Constant" : "Top");
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
            // Phi reads from its shadow projection
            return forValue(ValueFlowProjection(value, ValueFlowProjection::Shadow));
        }

        case Upsilon: {
            // Upsilon: return the child's value
            return forValue(value->child(0));
        }

        case Identity:
        case Opaque:
            return forValue(value->child(0));

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

    AbstractValue& forValue(ValueFlowProjection projection)
    {
        return m_abstractValues.at(projection);
    }

    AbstractValue& forValue(Value* value)
    {
        // Always constructs Primary projection, matching DFG (see DFGNodeFlowProjection.h line 44-48)
        return forValue(ValueFlowProjection(value));
    }

    AbstractValue computeBinaryArithmetic(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        // Type must match for arithmetic
        if (left.type() != right.type())
            return AbstractValue::top();

        switch (value->opcode()) {
        case Add:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() + right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() + right.int64Value());
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() + right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() + right.doubleValue());
            break;

        case Sub:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() - right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() - right.int64Value());
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() - right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() - right.doubleValue());
            break;

        case Mul:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() * right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() * right.int64Value());
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() * right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() * right.doubleValue());
            break;

        case Div:
            // Avoid division by zero
            if (left.type() == Int32 && right.int32Value() != 0)
                return AbstractValue::fromInt32(left.int32Value() / right.int32Value());
            if (left.type() == Int64 && right.int64Value() != 0)
                return AbstractValue::fromInt64(left.int64Value() / right.int64Value());
            if (left.type() == Float)
                return AbstractValue::fromFloat(left.floatValue() / right.floatValue());
            if (left.type() == Double)
                return AbstractValue::fromDouble(left.doubleValue() / right.doubleValue());
            break;

        default:
            break;
        }

        // Conservative fallback
        return AbstractValue::top();
    }

    AbstractValue computeBitwise(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        if (left.type() != right.type())
            return AbstractValue::top();

        switch (value->opcode()) {
        case BitAnd:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() & right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() & right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits & rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits & rightBits));
            }
            break;

        case BitOr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() | right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() | right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits | rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits | rightBits));
            }
            break;

        case BitXor:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() ^ right.int32Value());
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() ^ right.int64Value());
            if (left.type() == Float) {
                uint32_t leftBits = std::bit_cast<uint32_t>(left.floatValue());
                uint32_t rightBits = std::bit_cast<uint32_t>(right.floatValue());
                return AbstractValue::fromFloat(std::bit_cast<float>(leftBits ^ rightBits));
            }
            if (left.type() == Double) {
                uint64_t leftBits = std::bit_cast<uint64_t>(left.doubleValue());
                uint64_t rightBits = std::bit_cast<uint64_t>(right.doubleValue());
                return AbstractValue::fromDouble(std::bit_cast<double>(leftBits ^ rightBits));
            }
            break;

        case Shl:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() << (right.int32Value() & 31));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() << (right.int64Value() & 63));
            break;

        case SShr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(left.int32Value() >> (right.int32Value() & 31));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(left.int64Value() >> (right.int64Value() & 63));
            break;

        case ZShr:
            if (left.type() == Int32)
                return AbstractValue::fromInt32(static_cast<uint32_t>(left.int32Value()) >> (right.int32Value() & 31));
            if (left.type() == Int64)
                return AbstractValue::fromInt64(static_cast<uint64_t>(left.int64Value()) >> (right.int64Value() & 63));
            break;

        default:
            break;
        }

        return AbstractValue::top();
    }

    AbstractValue computeComparison(Value* value)
    {
        AbstractValue left = forValue(value->child(0));
        AbstractValue right = forValue(value->child(1));

        if (left.isBottom() || right.isBottom())
            return AbstractValue::bottom();

        if (left.isTop() || right.isTop())
            return AbstractValue::top();

        // Both are constants
        ASSERT(left.isConstant() && right.isConstant());

        if (left.type() != right.type())
            return AbstractValue::top();

        bool result = false;
        switch (value->opcode()) {
        case Equal:
            if (left.type() == Int32)
                result = left.int32Value() == right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() == right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() == right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() == right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case NotEqual:
            if (left.type() == Int32)
                result = left.int32Value() != right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() != right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() != right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() != right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case LessThan:
            if (left.type() == Int32)
                result = left.int32Value() < right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() < right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() < right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() < right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case GreaterThan:
            if (left.type() == Int32)
                result = left.int32Value() > right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() > right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() > right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() > right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case LessEqual:
            if (left.type() == Int32)
                result = left.int32Value() <= right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() <= right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() <= right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() <= right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case GreaterEqual:
            if (left.type() == Int32)
                result = left.int32Value() >= right.int32Value();
            else if (left.type() == Int64)
                result = left.int64Value() >= right.int64Value();
            else if (left.type() == Float)
                result = left.floatValue() >= right.floatValue();
            else if (left.type() == Double)
                result = left.doubleValue() >= right.doubleValue();
            else
                return AbstractValue::top();
            break;

        case Above:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) > static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) > static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case Below:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) < static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) < static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case AboveEqual:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) >= static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) >= static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case BelowEqual:
            if (left.type() == Int32)
                result = static_cast<uint32_t>(left.int32Value()) <= static_cast<uint32_t>(right.int32Value());
            else if (left.type() == Int64)
                result = static_cast<uint64_t>(left.int64Value()) <= static_cast<uint64_t>(right.int64Value());
            else
                return AbstractValue::top();
            break;

        case EqualOrUnordered:
            if (left.type() == Float)
                result = (left.floatValue() == right.floatValue()) || std::isnan(left.floatValue()) || std::isnan(right.floatValue());
            else if (left.type() == Double)
                result = (left.doubleValue() == right.doubleValue()) || std::isnan(left.doubleValue()) || std::isnan(right.doubleValue());
            else
                return AbstractValue::top();
            break;

        default:
            return AbstractValue::top();
        }

        // Comparisons return Int32 (boolean)
        return AbstractValue::fromInt32(result ? 1 : 0);
    }

    bool applyOptimizations()
    {
        bool changed = false;

        // Replace values with constants (following DFG's ConstantFoldingPhase pattern)
        for (BasicBlock* block : m_proc) {
            // Skip unreachable blocks
            BlockState& blockState = m_blockStates[block];
            if (!blockState.hasVisited)
                continue;

            // Load block's valuesAtHead into global m_abstractValues (matching DFG line 122)
            beginBasicBlock(block);

            for (unsigned valueIndex = 0; valueIndex < block->size(); ++valueIndex) {
                Value* value = block->at(valueIndex);

                // Skip terminals and phis (handled separately)
                if (value->opcode() == Phi || value->opcode() == Upsilon)
                    continue;

                AbstractValue abstractValue = forValue(value);

                if (!abstractValue.isConstant())
                    continue;

                dataLogLnIf(B3SCCPInternal::verbose, "Replacing ", *value, " with constant");

                Value* replacement = nullptr;
                switch (abstractValue.type().kind()) {
                case Int32:
                    replacement = m_insertionSet.insertIntConstant(valueIndex, value, abstractValue.int32Value());
                    break;
                case Int64:
                    replacement = m_insertionSet.insertIntConstant(valueIndex, value, abstractValue.int64Value());
                    break;
                case Float: {
                    float floatValue = abstractValue.floatValue();
                    replacement = m_insertionSet.insertValue(valueIndex,
                        m_proc.addConstant(value->origin(), Float, std::bit_cast<uint32_t>(floatValue)));
                    break;
                }
                case Double: {
                    double doubleValue = abstractValue.doubleValue();
                    replacement = m_insertionSet.insertValue(valueIndex,
                        m_proc.addConstant(value->origin(), Double, std::bit_cast<uint64_t>(doubleValue)));
                    break;
                }
                default:
                    continue;
                }

                if (replacement) {
                    value->replaceWithIdentity(replacement);
                    changed = true;
                }
            }

            // Simplify branches with constant conditions
            Value* terminal = block->last();
            if (terminal && terminal->opcode() == Branch) {
                AbstractValue condition = forValue(terminal->child(0));
                if (condition.isConstant()) {
                    bool takeBranch = false;
                    if (condition.type() == Int32)
                        takeBranch = condition.int32Value() != 0;
                    else if (condition.type() == Int64)
                        takeBranch = condition.int64Value() != 0;

                    BasicBlock* target = takeBranch ? block->taken().block() : block->notTaken().block();
                    dataLogLnIf(B3SCCPInternal::verbose, "Replacing branch in ", *block, " with jump to ", *target);
                    terminal->replaceWithJump(block, target);
                    changed = true;
                }
            }

            // Reset block-local state (matching DFG line 1841)
            m_block = nullptr;

            // Execute insertions (matching DFG line 1842)
            m_insertionSet.execute(block);
        }

        if (changed) {
            m_proc.resetReachability();
            m_proc.invalidateCFG();
        }

        return changed;
    }

    Procedure& m_proc;
    BasicBlock* m_block { nullptr };

    // Global FlowMap (matching DFG's m_abstractValues)
    FlowMap<AbstractValue> m_abstractValues;

    // Per-block state (matching DFG's BasicBlock::SSAData)
    IndexMap<BasicBlock*, BlockState> m_blockStates;

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
