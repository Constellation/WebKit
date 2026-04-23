/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JITCompilation.h>
#include <JavaScriptCore/SIMDInfo.h>
#include <JavaScriptCore/WasmLimits.h>
#include <JavaScriptCore/WasmOps.h>
#include <JavaScriptCore/WasmSIMDOpcodes.h>
#include <JavaScriptCore/Width.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
#include <JavaScriptCore/B3Type.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

namespace Wasm {

class JSToWasmICCallee;
class RTT;
class SectionParser;
class TypeSectionState;

#define CREATE_ENUM_VALUE(name, id, ...) name = id,
enum class ExtSIMDOpType : uint32_t {
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

#define CREATE_CASE(name, ...) case ExtSIMDOpType::name: return #name ## _s;
inline ASCIILiteral makeString(ExtSIMDOpType op)
{
    switch (op) {
        FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

constexpr std::pair<size_t, size_t> countNumberOfWasmExtendedSIMDOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_EXT_SIMD_OPERATION(name, id, ...) \
    numberOfOpcodes++; \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_EXT_SIMD_OP(COUNT_EXT_SIMD_OPERATION)
#undef COUNT_EXT_SIMD_OPERATION
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredWasmExtendedSIMDOpcode(ExtSIMDOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtSIMDOpType::name:
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtSIMDOpType(PrintStream& out, ExtSIMDOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtSIMDOpType::name: out.print(#name); break;
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtSIMDOpTypeDump, ExtSIMDOpType, dumpExtSIMDOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmExtendedAtomicOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_EXT_ATOMIC_OP(name, id, ...) \
    numberOfOpcodes++;                      \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(COUNT_WASM_EXT_ATOMIC_OP);
#undef COUNT_WASM_EXT_ATOMIC_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredExtenedAtomicOpcode(ExtAtomicOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtAtomicOpType::name:
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtAtomicOpType(PrintStream& out, ExtAtomicOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtAtomicOpType::name: out.print(#name); break;
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtAtomicOpTypeDump, ExtAtomicOpType, dumpExtAtomicOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmGCOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_GC_OP(name, id, ...) \
    numberOfOpcodes++;                  \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_GC_OP(COUNT_WASM_GC_OP);
#undef COUNT_WASM_GC_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredGCOpcode(ExtGCOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtGCOpType::name:
    FOR_EACH_WASM_GC_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtGCOpType(PrintStream& out, ExtGCOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtGCOpType::name: out.print(#name); break;
    FOR_EACH_WASM_GC_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtGCOpTypeDump, ExtGCOpType, dumpExtGCOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmBaseOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_OP(name, id, ...) \
    numberOfOpcodes++;               \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_OP(COUNT_WASM_OP);
#undef COUNT_WASM_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredBaseOpcode(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case OpType::name:
    FOR_EACH_WASM_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpOpType(PrintStream& out, OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case OpType::name: out.print(#name); break;
    FOR_EACH_WASM_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(OpTypeDump, OpType, dumpOpType);

inline bool isCompareOpType(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case name: return true;
    FOR_EACH_WASM_COMPARE_UNARY_OP(CREATE_CASE)
    FOR_EACH_WASM_COMPARE_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return false;
    }
}

constexpr Type simdScalarType(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::v128:
        RELEASE_ASSERT_NOT_REACHED();
        return Types::Void;
    case SIMDLane::i64x2:
        return Types::I64;
    case SIMDLane::f64x2:
        return Types::F64;
    case SIMDLane::i8x16:
    case SIMDLane::i16x8:
    case SIMDLane::i32x4:
        return Types::I32;
    case SIMDLane::f32x4:
        return Types::F32;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

using FunctionArgCount = uint32_t;
using StructFieldCount = uint32_t;
using RecursionGroupCount = uint32_t;
using ProjectionIndex = uint32_t;
using DisplayCount = uint32_t;
using SupertypeCount = uint32_t;

ALWAYS_INLINE Width Type::width() const
{
    switch (kind) {
#define CREATE_CASE(name, id, b3type, inc, wasmName, width, ...) case TypeKind::name: return widthForBytes(width / 8);
    FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
#define CREATE_CASE(name, id, b3type, ...) case TypeKind::name: return b3type;
inline B3::Type toB3Type(Type type)
{
    switch (type.kind) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return B3::Void;
}
#undef CREATE_CASE
#endif

constexpr size_t typeKindSizeInBytes(TypeKind kind)
{
    switch (kind) {
    case TypeKind::I32:
    case TypeKind::F32: {
        return 4;
    }
    case TypeKind::I64:
    case TypeKind::F64: {
        return 8;
    }
    case TypeKind::V128:
        return 16;

    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Funcref:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        return sizeof(WriteBarrierBase<Unknown>);
    }
    case TypeKind::Array:
    case TypeKind::Func:
    case TypeKind::Struct:
    case TypeKind::Void:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Rec:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
    case TypeKind::I31ref: {
        break;
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

class RecursionGroup;
class Projection;
class Subtype;

// Subtype/RecursionGroup/Projection are parser-internal classes (each
// ThreadSafeRefCounted with its own m_rtt). They are not visible to
// consumers after canonicalization -- only the resulting RTT is.
//
// TypeIndex (uintptr_t) is overloaded to carry one of:
//   - Abstract heap type (small negative number representing a TypeKind).
//   - Bare RTT pointer (untagged, low bits all zero).
//   - Bare Subtype pointer (tag bit subtypeTagBit set, used in
//     RecursionGroup::types() to discriminate Subtype members from concrete
//     RTT members).
//   - Bare Projection pointer (tag bit projectionTagBit set, used in
//     Subtype::superTypes() for intra-rec-group supertype refs and in
//     Type::index for placeholder ref types in canonical RTT payloads).
//
// The two tag bits are independent — context determines which one is in
// effect.
static constexpr TypeIndex projectionTagBit = 1; // bit 0
static constexpr TypeIndex subtypeTagBit = 2;    // bit 1

inline TypeIndex tagAsProjectionRef(const Projection* p) { return std::bit_cast<TypeIndex>(p) | projectionTagBit; }
inline const Projection* untagProjectionRef(TypeIndex idx) { return std::bit_cast<const Projection*>(idx & ~projectionTagBit); }

inline TypeIndex tagAsSubtypeRef(const Subtype* s) { return std::bit_cast<TypeIndex>(s) | subtypeTagBit; }
inline const Subtype* untagSubtypeRef(TypeIndex idx) { return std::bit_cast<const Subtype*>(idx & ~subtypeTagBit); }

class Subtype final : public RefCounted<Subtype> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(Subtype);
    WTF_MAKE_NONCOPYABLE(Subtype);
    WTF_MAKE_NONMOVABLE(Subtype);
public:
    // Sentinel TypeIndex for "no type" / uninitialized slots (e.g. empty
    // FuncRefTable entries). The JIT's call_indirect path compares a
    // loaded RTT pointer against nullptr for the same purpose; this
    // numeric value exists for the few non-JIT sites that still work in
    // TypeIndex terms.
    static constexpr TypeIndex invalidIndex = 0;

    static size_t allocationSize(Checked<SupertypeCount> count) { return sizeof(Subtype) + count * sizeof(TypeIndex); }

    static RefPtr<Subtype> tryCreate(std::span<const TypeIndex>, Ref<const RTT> underlyingRTT, bool isFinal);

    bool cleanup();

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
    SupertypeCount supertypeCount() const { return m_supertypeCount; }
    bool isFinal() const { return m_final; }
    TypeIndex firstSuperType() const { return superTypes()[0]; }
    TypeIndex superType(SupertypeCount i) const { return superTypes()[i]; }
    const RTT& underlyingRTT() const LIFETIME_BOUND { return m_underlyingRTT; }
    std::span<const TypeIndex> superTypes() const { return { payload(), m_supertypeCount }; }
    const RTT* rtt() const { return m_rtt.get(); }
    bool hasRecursiveReference() const;
    bool isFinalType() const { return m_final; }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    void setRTT(Ref<const RTT> rtt) const
    {
        m_rtt = WTF::move(rtt);
    }

private:
    friend class TypeInformation;
    Subtype(std::span<const TypeIndex>, Ref<const RTT> underlyingRTT, bool isFinal);

    std::span<TypeIndex> mutableSuperTypes() { return { payload(), m_supertypeCount }; }
    TypeIndex* payload() const { return std::bit_cast<TypeIndex*>(this + 1); }

    mutable RefPtr<const RTT> m_rtt;
    bool m_final;
    Ref<const RTT> m_underlyingRTT;
    SupertypeCount m_supertypeCount;
};

// Parser-internal sum type for "the result of parsing one type entry".
// The variant alternatives correspond to what the parser can produce:
//
//   - Ref<const RTT>: Function/Struct/Array kinds (already canonical).
//   - Ref<Subtype>: explicit (sub ...) declaration.
//   - Ref<RecursionGroup>: (rec ...) declaration.
//   - Ref<Projection>: shorthand-recursive types are wrapped in a singleton
//     RecursionGroup + Projection pair during parsing.
class ParsedDef {
public:
    using Variant = WTF::Variant<Ref<const RTT>, Ref<Subtype>, Ref<RecursionGroup>, Ref<Projection>>;

    ParsedDef() = default;
    ParsedDef(Ref<const RTT>&& v) : m_v(Variant { WTF::move(v) }) { }
    ParsedDef(Ref<Subtype>&& v) : m_v(Variant { WTF::move(v) }) { }
    ParsedDef(Ref<RecursionGroup>&& v) : m_v(Variant { WTF::move(v) }) { }
    ParsedDef(Ref<Projection>&& v) : m_v(Variant { WTF::move(v) }) { }

    bool isRTT() const { return m_v && std::holds_alternative<Ref<const RTT>>(*m_v); }
    bool isSubtype() const { return m_v && std::holds_alternative<Ref<Subtype>>(*m_v); }
    bool isRecursionGroup() const { return m_v && std::holds_alternative<Ref<RecursionGroup>>(*m_v); }
    bool isProjection() const { return m_v && std::holds_alternative<Ref<Projection>>(*m_v); }

    const RTT& asRTT() const { return std::get<Ref<const RTT>>(*m_v).get(); }
    const Subtype& asSubtype() const { return std::get<Ref<Subtype>>(*m_v).get(); }
    const RecursionGroup& asRecursionGroup() const { return std::get<Ref<RecursionGroup>>(*m_v).get(); }
    const Projection& asProjection() const { return std::get<Ref<Projection>>(*m_v).get(); }

    Ref<const RTT> rttRef() const { return std::get<Ref<const RTT>>(*m_v); }
    Ref<Subtype> subtypeRef() const { return std::get<Ref<Subtype>>(*m_v); }
    Ref<RecursionGroup> recursionGroupRef() const { return std::get<Ref<RecursionGroup>>(*m_v); }
    Ref<Projection> projectionRef() const { return std::get<Ref<Projection>>(*m_v); }

    bool hasRecursiveReference() const;
    // Encoded TypeIndex (with the subtypeTagBit / projectionTagBit
    // conventions) suitable for use as a recursion group's typeIndex or as
    // the "index" of this parsed type in canonicalization keys.
    TypeIndex index() const;
    Ref<const RTT> canonicalRTT() const;

    bool operator!() const { return !m_v.has_value(); }
    explicit operator bool() const { return m_v.has_value(); }

private:
    std::optional<Variant> m_v;
};

class RecursionGroup final : public RefCounted<RecursionGroup> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(RecursionGroup);
    WTF_MAKE_NONCOPYABLE(RecursionGroup);
    WTF_MAKE_NONMOVABLE(RecursionGroup);
public:
    static size_t allocationSize(Checked<RecursionGroupCount> typeCount) { return sizeof(RecursionGroup) + typeCount * sizeof(TypeIndex); }

    // Members in `types` are TypeIndex values: either bare RTT* (untagged)
    // for concrete-kind members, or tagged Subtype* (subtypeTagBit) for
    // Subtype members. The caller is responsible for setting the tag.
    static RefPtr<RecursionGroup> tryCreate(std::span<const TypeIndex>);

    bool cleanup();

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
    RecursionGroupCount typeCount() const { return m_typeCount; }
    TypeIndex type(RecursionGroupCount i) const { return types()[i]; }
    std::span<const TypeIndex> types() const { return { payload(), m_typeCount }; }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

private:
    friend class TypeInformation;
    explicit RecursionGroup(std::span<const TypeIndex>);

    std::span<TypeIndex> mutableTypes() { return { payload(), m_typeCount }; }
    TypeIndex* payload() const { return std::bit_cast<TypeIndex*>(this + 1); }

    RecursionGroupCount m_typeCount;
};

// A projection into a recursion group. m_recursionGroup is null for
// placeholders (intra-rec-group refs created at parse time before the actual
// group exists); after substitution it points to the real RecursionGroup.
class Projection final : public RefCounted<Projection> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(Projection);
    WTF_MAKE_NONCOPYABLE(Projection);
    WTF_MAKE_NONMOVABLE(Projection);
public:
    static size_t allocationSize() { return sizeof(Projection); }

    // recursionGroup TypeIndex: pass 0 for placeholders, otherwise pass the
    // RecursionGroup*'s index (untagged).
    static RefPtr<Projection> tryCreate(TypeIndex recursionGroup, ProjectionIndex index);

    bool cleanup();

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
    TypeIndex recursionGroup() const { return m_recursionGroup; }
    ProjectionIndex projectionIndex() const { return m_projectionIndex; }
    const RTT* rtt() const { return m_rtt.get(); }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    static constexpr TypeIndex PlaceholderGroup = 0;
    bool isPlaceholder() const { return recursionGroup() == PlaceholderGroup; }

    void setRTT(Ref<const RTT> rtt) const
    {
        m_rtt = WTF::move(rtt);
    }

private:
    friend class TypeInformation;
    Projection(TypeIndex recursionGroup, ProjectionIndex index);

    mutable RefPtr<const RTT> m_rtt;
    TypeIndex m_recursionGroup;
    ProjectionIndex m_projectionIndex;
};

// FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
enum Mutability : uint8_t {
    Mutable = 1,
    Immutable = 0
};

struct StorageType {
public:
    template <typename T>
    bool is() const { return std::holds_alternative<T>(m_storageType); }

    template <typename T>
    const T as() const { ASSERT(is<T>()); return *std::get_if<T>(&m_storageType); }

    StorageType() = default;

    explicit StorageType(Type t)
    {
        m_storageType = Variant<Type, PackedType>(t);
    }

    explicit StorageType(PackedType t)
    {
        m_storageType = Variant<Type, PackedType>(t);
    }

    // Return a value type suitable for validating instruction arguments. Packed types cannot show up as value types and need to be unpacked to I32.
    Type unpacked() const
    {
        if (is<Type>())
            return as<Type>();
        return Types::I32;
    }

    size_t elementSize() const
    {
        if (is<Type>()) {
            switch (as<Type>().kind) {
            case Wasm::TypeKind::I32:
            case Wasm::TypeKind::F32:
                return sizeof(uint32_t);
            case Wasm::TypeKind::I64:
            case Wasm::TypeKind::F64:
            case Wasm::TypeKind::Ref:
            case Wasm::TypeKind::RefNull:
                return sizeof(uint64_t);
            case Wasm::TypeKind::V128:
                return sizeof(v128_t);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        switch (as<PackedType>()) {
        case PackedType::I8:
            return sizeof(uint8_t);
        case PackedType::I16:
            return sizeof(uint16_t);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const StorageType& rhs) const
    {
        if (rhs.is<PackedType>())
            return (is<PackedType>() && as<PackedType>() == rhs.as<PackedType>());
        if (!is<Type>())
            return false;
        return(as<Type>() == rhs.as<Type>());
    }

    int8_t typeCode() const
    {
        if (is<Type>())
            return static_cast<int8_t>(as<Type>().kind);
        return static_cast<int8_t>(as<PackedType>());
    }

    TypeIndex index() const
    {
        if (is<Type>())
            return as<Type>().index;
        return 0;
    }
    void dump(WTF::PrintStream& out) const;

private:
    Variant<Type, PackedType> m_storageType;

};

inline ASCIILiteral makeString(const StorageType& storageType)
{
    return(storageType.is<Type>() ? makeString(storageType.as<Type>().kind) :
        makeString(storageType.as<PackedType>()));
}

inline size_t typeSizeInBytes(const StorageType& storageType)
{
    if (storageType.is<PackedType>()) {
        switch (storageType.as<PackedType>()) {
        case PackedType::I8: {
            return 1;
        }
        case PackedType::I16: {
            return 2;
        }
        }
    }
    return typeKindSizeInBytes(storageType.as<Type>().kind);
}

inline size_t typeAlignmentInBytes(const StorageType& storageType)
{
    return typeSizeInBytes(storageType);
}

class FieldType {
public:
    StorageType type;
    Mutability mutability;

    friend bool operator==(const FieldType&, const FieldType&) = default;
};

// Paired field + offset entry. Collapses what used to be two parallel
// FixedVectors in RTTStructPayload (fields + field offsets) into a single
// allocation. Costs one alignment-induced padding word per field but saves
// a heap allocation per struct RTT -- a measurable win during type-section
// parse given how many struct RTTs are built.
struct StructFieldEntry {
    FieldType type;
    unsigned offset;
};

// An RTT encodes subtyping information in a way that is suitable for executing
// runtime subtyping checks, e.g., for ref.cast and related operations. RTTs are also
// used to facilitate static subtyping checks for references.
//
// It contains a display data structure that allows subtyping of references to be checked in constant time.
//
// See https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#runtime-types for an explanation of displays.
enum class RTTKind : uint8_t {
    Function,
    Array,
    Struct
};

// RTT*Payload holds the structural data for a concrete type (function,
// struct, or array). One of the three is stored inline in RTT via Variant,
// chosen by RTTKind.
class RTTFunctionPayload {
    WTF_MAKE_NONCOPYABLE(RTTFunctionPayload);
public:
    RTTFunctionPayload() = default;
    RTTFunctionPayload(RTTFunctionPayload&&) = default;
    RTTFunctionPayload& operator=(RTTFunctionPayload&&) = default;

    RTTFunctionPayload(FunctionArgCount argCount, FunctionArgCount retCount, std::span<const Type> signatureReturnsThenArgs, bool includesI64, bool includesV128, bool includesExnref, bool hasRecursiveReference)
        : m_signature(signatureReturnsThenArgs)
        , m_argCount(argCount)
        , m_retCount(retCount)
        , m_argumentsOrResultsIncludeI64(includesI64)
        , m_argumentsOrResultsIncludeV128(includesV128)
        , m_argumentsOrResultsIncludeExnref(includesExnref)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
        ASSERT(m_signature.size() == static_cast<size_t>(m_argCount) + static_cast<size_t>(m_retCount));
    }

    // Move-in constructor: caller has already built a FixedVector<Type> for
    // the signature (returns then args). Avoids the span-copy performed by
    // the span-based constructor. Used by provider-based typeDefinitionFor*
    // paths that populate the FixedVector in place.
    RTTFunctionPayload(FunctionArgCount argCount, FunctionArgCount retCount, FixedVector<Type>&& signatureReturnsThenArgs, bool includesI64, bool includesV128, bool includesExnref, bool hasRecursiveReference)
        : m_signature(WTF::move(signatureReturnsThenArgs))
        , m_argCount(argCount)
        , m_retCount(retCount)
        , m_argumentsOrResultsIncludeI64(includesI64)
        , m_argumentsOrResultsIncludeV128(includesV128)
        , m_argumentsOrResultsIncludeExnref(includesExnref)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
        ASSERT(m_signature.size() == static_cast<size_t>(m_argCount) + static_cast<size_t>(m_retCount));
    }

    FunctionArgCount argumentCount() const { return m_argCount; }
    FunctionArgCount returnCount() const { return m_retCount; }
    Type returnType(FunctionArgCount i) const { ASSERT(i < m_retCount); return m_signature[i]; }
    Type argumentType(FunctionArgCount i) const { ASSERT(i < m_argCount); return m_signature[m_retCount + i]; }
    bool returnsVoid() const { return !m_retCount; }
    bool argumentsOrResultsIncludeI64() const { return m_argumentsOrResultsIncludeI64; }
    bool argumentsOrResultsIncludeV128() const { return m_argumentsOrResultsIncludeV128; }
    bool argumentsOrResultsIncludeExnref() const { return m_argumentsOrResultsIncludeExnref; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    std::span<const Type> signatureSpan() const LIFETIME_BOUND { return m_signature.span(); }

    // Used during recgroup canonicalization to rewrite internal refs in place.
    template<typename Rewriter>
    void rewriteSignature(Rewriter&& rewrite)
    {
        for (size_t i = 0; i < m_signature.size(); ++i)
            m_signature[i] = rewrite(m_signature[i]);
    }

    // Keep referenced RTTs alive (cycles allowed -- see "Cyclic RTT graph"
    // note on RTT class). The payload's Type::index for ref types may be
    // a bare RTT* into the canonical recgroup graph; without this anchor
    // we'd UAF when the referenced RTT loses its other owners.
    void addReferencedRTT(Ref<const RTT> rtt) { m_referencedRTTs.append(WTF::move(rtt)); }
    std::span<const Ref<const RTT>> referencedRTTs() const LIFETIME_BOUND { return m_referencedRTTs.span(); }
    // Drops the m_referencedRTTs anchors. Used by canonicalization-discard
    // paths to break the refcount cycle that rewriteInternalRefs introduces
    // for intra-recgroup / self-referential candidates. See
    // RTT::clearReferencedRTTs.
    void clearReferencedRTTs() { m_referencedRTTs.clear(); }

private:
    FixedVector<Type> m_signature;
    Vector<Ref<const RTT>> m_referencedRTTs;
    FunctionArgCount m_argCount { 0 };
    FunctionArgCount m_retCount { 0 };
    bool m_argumentsOrResultsIncludeI64 : 1 { false };
    bool m_argumentsOrResultsIncludeV128 : 1 { false };
    bool m_argumentsOrResultsIncludeExnref : 1 { false };
    bool m_hasRecursiveReference : 1 { false };
};

class RTTStructPayload {
    WTF_MAKE_NONCOPYABLE(RTTStructPayload);
public:
    RTTStructPayload() = default;
    RTTStructPayload(RTTStructPayload&&) = default;
    RTTStructPayload& operator=(RTTStructPayload&&) = default;

    // Move-in constructor: caller has prebuilt the FixedVector<StructFieldEntry>
    // (fields + offsets colocated). One heap allocation per struct RTT.
    RTTStructPayload(FixedVector<StructFieldEntry>&& fields, size_t instancePayloadSize, bool hasRefFieldTypes, bool hasRecursiveReference)
        : m_fields(WTF::move(fields))
        , m_instancePayloadSize(instancePayloadSize)
        , m_hasRefFieldTypes(hasRefFieldTypes)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
    }

    StructFieldCount fieldCount() const { return m_fields.size(); }
    const FieldType& field(StructFieldCount i) const LIFETIME_BOUND { return m_fields[i].type; }
    unsigned offsetOfFieldInPayload(StructFieldCount i) const { return m_fields[i].offset; }
    size_t instancePayloadSize() const { return m_instancePayloadSize; }
    bool hasRefFieldTypes() const { return m_hasRefFieldTypes; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    std::span<const StructFieldEntry> fieldsSpan() const LIFETIME_BOUND { return m_fields.span(); }

    // Used during recgroup canonicalization to rewrite internal refs in place.
    template<typename Rewriter>
    void rewriteFields(Rewriter&& rewrite)
    {
        for (size_t i = 0; i < m_fields.size(); ++i) {
            FieldType& f = m_fields[i].type;
            if (f.type.is<Type>())
                f.type = StorageType(rewrite(f.type.as<Type>()));
        }
    }

    // Keep referenced RTTs alive. See RTTFunctionPayload::addReferencedRTT.
    void addReferencedRTT(Ref<const RTT> rtt) { m_referencedRTTs.append(WTF::move(rtt)); }
    std::span<const Ref<const RTT>> referencedRTTs() const LIFETIME_BOUND { return m_referencedRTTs.span(); }
    void clearReferencedRTTs() { m_referencedRTTs.clear(); }

private:
    FixedVector<StructFieldEntry> m_fields;
    Vector<Ref<const RTT>> m_referencedRTTs;
    size_t m_instancePayloadSize { 0 };
    bool m_hasRefFieldTypes : 1 { false };
    bool m_hasRecursiveReference : 1 { false };
};

class RTTArrayPayload {
    WTF_MAKE_NONCOPYABLE(RTTArrayPayload);
public:
    RTTArrayPayload() = default;
    RTTArrayPayload(RTTArrayPayload&&) = default;
    RTTArrayPayload& operator=(RTTArrayPayload&&) = default;

    RTTArrayPayload(FieldType elementType, bool hasRecursiveReference)
        : m_elementType(elementType)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
    }

    const FieldType& elementType() const LIFETIME_BOUND { return m_elementType; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    // Used during recgroup canonicalization to rewrite internal refs in place.
    template<typename Rewriter>
    void rewriteElementType(Rewriter&& rewrite)
    {
        if (m_elementType.type.is<Type>())
            m_elementType.type = StorageType(rewrite(m_elementType.type.as<Type>()));
    }

    // Keep referenced RTTs alive. See RTTFunctionPayload::addReferencedRTT.
    void addReferencedRTT(Ref<const RTT> rtt) { m_referencedRTTs.append(WTF::move(rtt)); }
    std::span<const Ref<const RTT>> referencedRTTs() const LIFETIME_BOUND { return m_referencedRTTs.span(); }
    void clearReferencedRTTs() { m_referencedRTTs.clear(); }

private:
    FieldType m_elementType { };
    Vector<Ref<const RTT>> m_referencedRTTs;
    bool m_hasRecursiveReference { false };
};

class alignas(16) RTT final : public ThreadSafeRefCounted<RTT>, private TrailingArray<RTT, RefPtr<const RTT>> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(RTT);
    WTF_MAKE_NONMOVABLE(RTT);
    using TrailingArrayType = TrailingArray<RTT, RefPtr<const RTT>>;
    friend TrailingArrayType;
public:
    static_assert(sizeof(const RTT*) == sizeof(RefPtr<const RTT>));
    static constexpr unsigned inlinedDisplaySize = 6;
    RTT() = delete;
    ~RTT();

    static RefPtr<RTT> tryCreate(RTTKind, bool isFinalType, StructFieldCount fieldCount);
    static RefPtr<RTT> tryCreate(RTTKind, const RTT&, bool isFinalType, StructFieldCount fieldCount);
    static RefPtr<RTT> tryCreateFunction(bool isFinalType, RTTFunctionPayload&&);
    static RefPtr<RTT> tryCreateFunction(const RTT& supertype, bool isFinalType, RTTFunctionPayload&&);
    static RefPtr<RTT> tryCreateStruct(bool isFinalType, RTTStructPayload&&);
    static RefPtr<RTT> tryCreateStruct(const RTT& supertype, bool isFinalType, RTTStructPayload&&);
    static RefPtr<RTT> tryCreateArray(bool isFinalType, RTTArrayPayload&&);
    static RefPtr<RTT> tryCreateArray(const RTT& supertype, bool isFinalType, RTTArrayPayload&&);

    RTTKind kind() const { return m_kind; }
    DisplayCount displaySizeExcludingThis() const { return m_displaySizeExcludingThis; }
    const RTT* displayEntry(DisplayCount i) const { return at(i).get(); }
    StructFieldCount fieldCount() const { return m_fieldCount; }

    // View this RTT pointer as a TypeIndex (concrete encoding: bit-cast of the RTT pointer).
    // Use when a Category-C API takes `TypeIndex`; every concrete TypeIndex is bit-identical
    // to `const RTT*`.
    TypeIndex asTypeIndex() const { return std::bit_cast<TypeIndex>(this); }

    bool hasFunctionPayload() const { return std::holds_alternative<RTTFunctionPayload>(m_payload); }
    bool hasStructPayload() const { return std::holds_alternative<RTTStructPayload>(m_payload); }
    bool hasArrayPayload() const { return std::holds_alternative<RTTArrayPayload>(m_payload); }
    const RTTFunctionPayload& functionPayload() const LIFETIME_BOUND { return std::get<RTTFunctionPayload>(m_payload); }
    const RTTStructPayload& structPayload() const LIFETIME_BOUND { return std::get<RTTStructPayload>(m_payload); }
    const RTTArrayPayload& arrayPayload() const LIFETIME_BOUND { return std::get<RTTArrayPayload>(m_payload); }

    // Function payload accessors.
    FunctionArgCount argumentCount() const { return functionPayload().argumentCount(); }
    FunctionArgCount returnCount() const { return functionPayload().returnCount(); }
    Type argumentType(FunctionArgCount i) const { return functionPayload().argumentType(i); }
    Type returnType(FunctionArgCount i) const { return functionPayload().returnType(i); }
    bool returnsVoid() const { return functionPayload().returnsVoid(); }
    bool argumentsOrResultsIncludeI64() const { return functionPayload().argumentsOrResultsIncludeI64(); }
    bool argumentsOrResultsIncludeV128() const { return functionPayload().argumentsOrResultsIncludeV128(); }
    bool argumentsOrResultsIncludeExnref() const { return functionPayload().argumentsOrResultsIncludeExnref(); }

    size_t numVectors() const
    {
        size_t n = 0;
        for (FunctionArgCount i = 0; i < argumentCount(); ++i) {
            if (argumentType(i).isV128())
                ++n;
        }
        return n;
    }

    size_t numReturnVectors() const
    {
        size_t n = 0;
        for (FunctionArgCount i = 0; i < returnCount(); ++i) {
            if (returnType(i).isV128())
                ++n;
        }
        return n;
    }

    bool hasReturnVector() const
    {
        for (FunctionArgCount i = 0; i < returnCount(); ++i) {
            if (returnType(i).isV128())
                return true;
        }
        return false;
    }

    // Struct payload accessors.
    const FieldType& field(StructFieldCount i) const LIFETIME_BOUND
    {
        ASSERT(m_kind == RTTKind::Struct);
        ASSERT(hasStructPayload());
        return structPayload().field(i);
    }
    unsigned offsetOfFieldInPayload(StructFieldCount i) const { return structPayload().offsetOfFieldInPayload(i); }
    size_t instancePayloadSize() const { return structPayload().instancePayloadSize(); }
    bool hasRefFieldTypes() const { return structPayload().hasRefFieldTypes(); }

    // Array payload accessors.
    const FieldType& elementType() const LIFETIME_BOUND { return arrayPayload().elementType(); }

    // Returns whether the payload references a type within its own recursion group.
    bool hasRecursiveReference() const
    {
        switch (m_kind) {
        case RTTKind::Function:
            return functionPayload().hasRecursiveReference();
        case RTTKind::Struct:
            return structPayload().hasRecursiveReference();
        case RTTKind::Array:
            return arrayPayload().hasRecursiveReference();
        }
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    const RTT& definingRTTForField(StructFieldCount fieldIndex) const
    {
        ASSERT(m_kind == RTTKind::Struct);
        ASSERT(fieldIndex < m_fieldCount);
        for (DisplayCount i = 0; i < m_displaySizeExcludingThis; ++i) {
            const RTT* ancestor = displayEntry(i);
            if (ancestor->kind() == RTTKind::Struct && fieldIndex < ancestor->fieldCount())
                return *ancestor;
        }
        return *this;
    }

    // Generate a key for each field, which can be usable for B3 TBAA.
    uint64_t fieldHeapKey(StructFieldCount fieldIndex) const
    {
        uint64_t ptr = std::bit_cast<uintptr_t>(this);
#if CPU(ADDRESS64)
        static_assert(maxStructFieldCount <= (1U << 20));
        constexpr uint32_t fieldIndexMask = (1 << 20) - 1;
        uint32_t maskedFieldIndex = fieldIndex & fieldIndexMask; // mod 20-bits.
        return static_cast<uint64_t>(ptr | (maskedFieldIndex & 0b1111) | (static_cast<uint64_t>(maskedFieldIndex >> 4) << 48));
#else
        return static_cast<uint64_t>(ptr | (static_cast<uint64_t>(fieldIndex) << 32));
#endif
    }

    bool NODELETE isSubRTT(const RTT& other) const;
    bool NODELETE isStrictSubRTT(const RTT& other) const;
    bool isFinalType() const { return m_isFinalType; }

    // Print this RTT for debugging.
    String toString() const;
    void dump(WTF::PrintStream& out) const;

#if ENABLE(JIT)
    // For function-kind RTTs: returns the JS->Wasm IC entrypoint for this signature.
    // The IC cache + lock live on the RTT itself.
    CodePtr<JSEntryPtrTag> jsToWasmICEntrypoint() const;
#endif

    void setPayload(RTTFunctionPayload&& payload) { m_payload = WTF::move(payload); }
    void setPayload(RTTStructPayload&& payload) { m_payload = WTF::move(payload); }
    void setPayload(RTTArrayPayload&& payload) { m_payload = WTF::move(payload); }

    // Rewrite internal recgroup refs in this RTT's payload to canonical RTT
    // pointers. Called during canonicalizeRecursionGroup before the candidate
    // RTTs are exposed: any Type ref whose index is a Projection of
    // recursionGroupIndex is replaced with groupMembers[projectionIndex] (i.e.,
    // a self-reference in the post-canonicalization group). After this all
    // refs in payload are uniformly RTT* form.
    void rewriteInternalRefs(TypeSectionState*, const Vector<Ref<const RTT>>& groupMembers, TypeIndex recursionGroupIndex);

    // Drop all RTT anchors added by rewriteInternalRefs (and by
    // typeDefinitionFor* for external refs). Used when a freshly built
    // candidate RTT turns out to duplicate an existing canonical entry
    // during canonicalization: clearing the payload's m_referencedRTTs
    // breaks the intra-recgroup refcount cycle so the discarded candidate
    // can actually be freed. Never call on a committed canonical RTT --
    // doing so would re-introduce the UAF rewriteInternalRefs guards
    // against.
    void clearReferencedRTTs();

    // Set during canonicalization (under TypeInformation::m_lock) to identify
    // membership in a canonical recursion group without scanning the group's
    // members vector. 0 = not yet canonicalized. indexInGroup is the relative
    // projection index (0 for singletons, 0..size-1 for multi-member).
    // Mirrors V8's CanonicalTypeIndex + RecursionGroupRange arithmetic.
    void setCanonicalGroup(uint32_t groupId, uint32_t indexInGroup) const
    {
        m_canonicalGroupId = groupId;
        m_canonicalIndexInGroup = indexInGroup;
    }
    uint32_t canonicalGroupId() const { return m_canonicalGroupId; }
    uint32_t canonicalIndexInGroup() const { return m_canonicalIndexInGroup; }

    static constexpr ptrdiff_t offsetOfKind() { return OBJECT_OFFSETOF(RTT, m_kind); }
    static constexpr ptrdiff_t offsetOfDisplaySizeExcludingThis() { return OBJECT_OFFSETOF(RTT, m_displaySizeExcludingThis); }
    using TrailingArrayType::offsetOfData;

private:
    explicit RTT(RTTKind kind, bool isFinalType, StructFieldCount fieldCount);
    RTT(RTTKind, const RTT& supertype, bool isFinalType, StructFieldCount fieldCount);

    const RTTKind m_kind;
    const bool m_isFinalType { false };
    const unsigned m_displaySizeExcludingThis { };
    const StructFieldCount m_fieldCount { 0 };
    mutable uint32_t m_canonicalGroupId { 0 };
    mutable uint32_t m_canonicalIndexInGroup { 0 };
#if ENABLE(JIT)
    // Cache for the JS->Wasm IC entrypoint. Function-kind only; null for
    // struct/array. Lazy-initialized under m_jitCodeLock; once set, the IC
    // pointer is read without locking via the storeStoreFence in the writer.
    mutable RefPtr<JSToWasmICCallee> m_jsToWasmICCallee;
    mutable Lock m_jitCodeLock;
#endif
    Variant<std::monostate, RTTFunctionPayload, RTTStructPayload, RTTArrayPayload> m_payload;
};

inline void Type::dump(PrintStream& out) const
{
    TypeKind kindToPrint = kind;
    if (index != Subtype::invalidIndex) {
        if (typeIndexIsType(index)) {
            // If the index is negative, it represents a TypeKind.
            kindToPrint = static_cast<TypeKind>(index);
        } else if (index & projectionTagBit) {
            out.print(*untagProjectionRef(index));
            return;
        } else {
            out.print(*reinterpret_cast<const RTT*>(index));
            return;
        }
    }
    switch (kindToPrint) {
#define CREATE_CASE(name, ...) case TypeKind::name: out.print(#name); break;
        FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
}

// Hash key for canonicalizing Subtype objects in TypeInformation::m_subtypes.
struct SubtypeHash {
    RefPtr<Subtype> key { nullptr };
    SubtypeHash() = default;
    explicit SubtypeHash(Ref<Subtype>&& key) : key(WTF::move(key)) { }
    explicit SubtypeHash(WTF::HashTableDeletedValueType) : key(WTF::HashTableDeletedValue) { }
    bool operator==(const SubtypeHash& rhs) const { return key == rhs.key; }
    static bool equal(const SubtypeHash& lhs, const SubtypeHash& rhs) { return lhs.key == rhs.key; }
    static unsigned hash(const SubtypeHash&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    bool isHashTableDeletedValue() const { return key.isHashTableDeletedValue(); }
};

// Hash key for canonicalizing RecursionGroup objects.
struct RecursionGroupHash {
    RefPtr<RecursionGroup> key { nullptr };
    RecursionGroupHash() = default;
    explicit RecursionGroupHash(Ref<RecursionGroup>&& key) : key(WTF::move(key)) { }
    explicit RecursionGroupHash(WTF::HashTableDeletedValueType) : key(WTF::HashTableDeletedValue) { }
    bool operator==(const RecursionGroupHash& rhs) const { return key == rhs.key; }
    static bool equal(const RecursionGroupHash& lhs, const RecursionGroupHash& rhs) { return lhs.key == rhs.key; }
    static unsigned hash(const RecursionGroupHash&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    bool isHashTableDeletedValue() const { return key.isHashTableDeletedValue(); }
};

// Hash key for canonicalizing Projection objects.
struct ProjectionHash {
    RefPtr<Projection> key { nullptr };
    ProjectionHash() = default;
    explicit ProjectionHash(Ref<Projection>&& key) : key(WTF::move(key)) { }
    explicit ProjectionHash(WTF::HashTableDeletedValueType) : key(WTF::HashTableDeletedValue) { }
    bool operator==(const ProjectionHash& rhs) const { return key == rhs.key; }
    static bool equal(const ProjectionHash& lhs, const ProjectionHash& rhs) { return lhs.key == rhs.key; }
    static unsigned hash(const ProjectionHash&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    bool isHashTableDeletedValue() const { return key.isHashTableDeletedValue(); }
};

// Isorecursive canonical recursion-group entry. groupId is a globally-unique
// id assigned during canonicalization; each member RTT carries the same
// groupId in its m_canonicalGroupId field, so intra-group membership is
// tested by a single id comparison instead of a HashMap/Vector scan. Mirrors
// V8's CanonicalTypeIndex + RecursionGroupRange.
struct CanonicalRecursionGroupEntry {
    TypeIndex recursionGroupIndex { 0 };
    uint32_t groupId { 0 };
    Vector<Ref<const RTT>> rtts;

    CanonicalRecursionGroupEntry() = default;
    CanonicalRecursionGroupEntry(TypeIndex idx, uint32_t id, Vector<Ref<const RTT>>&& r)
        : recursionGroupIndex(idx)
        , groupId(id)
        , rtts(WTF::move(r))
    { }
    explicit CanonicalRecursionGroupEntry(WTF::HashTableDeletedValueType)
        : recursionGroupIndex(std::numeric_limits<TypeIndex>::max())
    { }
    bool isHashTableDeletedValue() const { return recursionGroupIndex == std::numeric_limits<TypeIndex>::max(); }
    bool operator==(const CanonicalRecursionGroupEntry& other) const;
};

struct CanonicalRecursionGroupEntryHash {
    static unsigned hash(const CanonicalRecursionGroupEntry&);
    static bool equal(const CanonicalRecursionGroupEntry&, const CanonicalRecursionGroupEntry&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

// Canonical entry for a size-1 recursion group. Fast-path analogue of
// CanonicalRecursionGroupEntry: hashed/compared by the single RTT's
// structural content, with self-refs detected by pointer equality against
// the entry's own RTT. Mirrors V8's canonical_singleton_groups_.
struct CanonicalSingletonEntry {
    RefPtr<const RTT> rtt;

    CanonicalSingletonEntry() = default;
    explicit CanonicalSingletonEntry(Ref<const RTT>&& r)
        : rtt(WTF::move(r))
    { }
    explicit CanonicalSingletonEntry(WTF::HashTableDeletedValueType)
        : rtt(WTF::HashTableDeletedValue)
    { }
    bool isHashTableDeletedValue() const { return rtt.isHashTableDeletedValue(); }
    bool operator==(const CanonicalSingletonEntry& other) const;
};

struct CanonicalSingletonEntryHash {
    static unsigned hash(const CanonicalSingletonEntry&);
    static bool equal(const CanonicalSingletonEntry&, const CanonicalSingletonEntry&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} } // namespace JSC::Wasm


namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Wasm::SubtypeHash> : JSC::Wasm::SubtypeHash { };
template<> struct DefaultHash<JSC::Wasm::RecursionGroupHash> : JSC::Wasm::RecursionGroupHash { };
template<> struct DefaultHash<JSC::Wasm::ProjectionHash> : JSC::Wasm::ProjectionHash { };
template<> struct DefaultHash<JSC::Wasm::CanonicalRecursionGroupEntry> : JSC::Wasm::CanonicalRecursionGroupEntryHash { };
template<> struct DefaultHash<JSC::Wasm::CanonicalSingletonEntry> : JSC::Wasm::CanonicalSingletonEntryHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Wasm::SubtypeHash> : SimpleClassHashTraits<JSC::Wasm::SubtypeHash> {
    static constexpr bool emptyValueIsZero = true;
};
template<> struct HashTraits<JSC::Wasm::RecursionGroupHash> : SimpleClassHashTraits<JSC::Wasm::RecursionGroupHash> {
    static constexpr bool emptyValueIsZero = true;
};
template<> struct HashTraits<JSC::Wasm::ProjectionHash> : SimpleClassHashTraits<JSC::Wasm::ProjectionHash> {
    static constexpr bool emptyValueIsZero = true;
};
template<> struct HashTraits<JSC::Wasm::CanonicalRecursionGroupEntry> : SimpleClassHashTraits<JSC::Wasm::CanonicalRecursionGroupEntry> {
    static constexpr bool emptyValueIsZero = false;
};
template<> struct HashTraits<JSC::Wasm::CanonicalSingletonEntry> : SimpleClassHashTraits<JSC::Wasm::CanonicalSingletonEntry> {
    static constexpr bool emptyValueIsZero = true;
};

} // namespace WTF


namespace JSC { namespace Wasm {

// Type information is held globally and shared by the entire process to allow all type definitions to be unique.
// This is required when wasm calls another wasm instance, and must work when modules are shared between multiple VMs.
// Isorecursive canonical recursion-group entry. The recursionGroupIndex is the
// TypeIndex of the RecursionGroup this entry was minted from;
// it is used at hash/equal time to determine which Type-references in the
// member RTTs' payloads are intra-group (encoded as relative projection index)
// versus external (encoded as a canonical RTT pointer). See canonicalizeRecursionGroup.
// Isorecursive canonical recursion-group entry. The recursionGroupIndex is the
// TypeIndex of the RecursionGroup this entry was minted from;
// it is used at hash/equal time to determine which Type-references in the
// member RTTs' payloads are intra-group (encoded as relative projection index)
// versus external (encoded as a canonical RTT pointer). See canonicalizeRecursionGroup.
class TypeInformation {
    WTF_MAKE_NONCOPYABLE(TypeInformation);
    WTF_MAKE_TZONE_ALLOCATED(TypeInformation);

    TypeInformation();

public:
    friend class ParserBase;
    friend class ParsedDef;
    friend class RTT;
    friend class SectionParser;
    friend class Subtype;
    friend class TypeSectionState;

    static TypeInformation& singleton();

    static const RTT& signatureForJSException();

    static Ref<const RTT> rttForFunction(const Vector<Type, 16>& returnTypes, const Vector<Type, 16>& argumentTypes);

    static bool isReferenceValueAssignable(JSValue, bool, TypeIndex);

    // External TypeIndex values for concrete types are RTT pointers (set up
    // by ModuleInformation::typeIndexFromTypeSignatureIndex / Tag::typeIndex /
    // WebAssemblyFunctionBase). This bit-casts directly, no lookup.
    static Ref<const RTT> getCanonicalRTT(TypeIndex);

    // The index passed to tryGetRTTRef may be an abstract type or
    // invalid, in which case nullptr is returned. The non-null result
    // assumes the index is a bare RTT pointer (the external/canonical
    // convention).
    static RefPtr<const RTT> tryGetRTT(TypeIndex);

    // Placeholder Projection encoding: parser-internal intra-recgroup refs
    // carry a tag bit so consumers can distinguish them from canonical RTT*
    // values without UB-prone bit_cast lookups. Exposed for probe/translator
    // paths in WasmTypeDefinition.cpp.
    static bool isPlaceholderRef(TypeIndex);

    // Allocate a fresh canonical group id. Used by translator-based probe
    // paths that insert a new canonical singleton and want to mark the RTT
    // as canonical so redundant canonicalizeSingleton calls can short
    // circuit. Caller must hold m_lock.
    uint32_t allocateCanonicalGroupId() { return m_nextCanonicalGroupId++; }

    static void tryCleanup();

private:
    static Ref<const RTT> typeDefinitionForFunction(const Vector<Type, 16>& returnTypes, const Vector<Type, 16>& argumentTypes);
    static Ref<const RTT> typeDefinitionForStruct(const Vector<FieldType>& fields);
    static Ref<const RTT> typeDefinitionForArray(FieldType);

    // Provider-based overloads: caller supplies a callable instead of a
    // Vector. The FixedVector inside the resulting RTT payload is built in
    // place via the provider, eliminating one heap allocation + one copy
    // compared to the Vector-based overloads. Used by
    // TypeSectionState::createCanonicalRTT's rebuild-with-substitution path
    // where the intermediate Vector was purely scaffolding.
    template<typename FieldProvider>
    static Ref<const RTT> typeDefinitionForStructFromProvider(StructFieldCount fieldCount, FieldProvider&& provider);
    template<typename ReturnProvider, typename ArgProvider>
    static Ref<const RTT> typeDefinitionForFunctionFromProviders(FunctionArgCount retCount, ReturnProvider&& returnsProvider, FunctionArgCount argCount, ArgProvider&& argsProvider);

    // Isorecursive RTT canonicalization at recursion-group granularity.
    // Given a freshly-parsed recursion group identified by recursionGroupIndex
    // and the per-member candidate RTTs auto-registered for it, return the
    // canonical RTT vector. If a structurally-identical recursion group is
    // already canonical, the candidates are discarded and the existing canonical
    // RTTs are returned (so cross-module identical recgroups share RTT identity).
    // Otherwise the candidates become canonical for this signature.
    //
    // Equality treats refs to projections of recursionGroupIndex by their
    // relative projection index; refs outside the group are compared by
    // canonical RTT pointer. Mirrors V8's CanonicalEquality.
    static Vector<Ref<const RTT>> canonicalizeRecursionGroup(TypeSectionState*, TypeIndex recursionGroupIndex, Vector<Ref<const RTT>>&& candidateRTTs);

    // Fast path for the common case of a single-member recursion group
    // (standalone struct/array/function, shorthand types, self-recursive
    // singletons). Bypasses the Vector<Ref<const RTT>> allocation and goes
    // directly to the singleton canonical table. recursionGroupIndex is the
    // original recursion-group TypeIndex used by placeholder projections in
    // the candidate's payload; pass 0 for non-recursive standalone types.
    static Ref<const RTT> canonicalizeSingleton(TypeSectionState*, TypeIndex recursionGroupIndex, Ref<const RTT>&& candidate);

    // Canonicalize a single non-recursive RTT (function/struct/array). If a
    // structurally-identical RTT is already canonical, returns it; otherwise the
    // input becomes canonical. Used by TypeInformation initialization for
    // built-in signatures (m_Void_Externref, thunks) so they share identity with
    // RTTs minted later by parseType.
    static Ref<const RTT> canonicalizeStandaloneRTT(Ref<const RTT>&&);

    // Non-static impls (avoid singleton() recursion when called from
    // TypeInformation's own constructor).
    Vector<Ref<const RTT>> canonicalizeRecursionGroupImpl(TypeSectionState*, TypeIndex, Vector<Ref<const RTT>>&&);
    Ref<const RTT> canonicalizeSingletonImpl(TypeSectionState*, TypeIndex recursionGroupIndex, Ref<const RTT>&&);
    Ref<const RTT> canonicalizeStandaloneRTTImpl(Ref<const RTT>&&);

    // Encode/decode placeholder TypeIndex values. Placeholders carry a tag bit
    // so isRefWithRecursiveReference can distinguish them from canonical RTT*
    // values without UB-prone bit_cast lookups.
    static TypeIndex placeholderRefIndex(const Projection&);
    static RefPtr<const RTT> extractExternalRTT(Type);

    // Returns true iff `type` is a ref whose index encodes a parser-time
    // placeholder Projection (i.e. an unresolved intra-recgroup reference).
    // Canonical (post-parse) Type::index values are RTT pointers and return
    // false. Centralised here so consumers don't need to know about the
    // placeholder-tag-bit encoding.
    static bool isRefWithRecursiveReference(Type);
    static bool isRefWithRecursiveReference(StorageType);

    UncheckedKeyHashSet<CanonicalRecursionGroupEntry, CanonicalRecursionGroupEntryHash> m_canonicalRecursionGroups;
    UncheckedKeyHashSet<CanonicalSingletonEntry> m_canonicalSingletonGroups;
    // Monotonically-allocated id for canonical groups (singleton and
    // multi-member). Each canonical RTT stores its group's id in
    // m_canonicalGroupId so membership checks during hash/equal are O(1)
    // integer comparisons. 0 is reserved for "not yet canonicalized".
    uint32_t m_nextCanonicalGroupId { 1 };
    RefPtr<const RTT> m_Void_Externref;
    Lock m_lock;
};

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
