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
#include "WasmTypeSectionState.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

namespace {

inline const Projection* projectionFromTaggedIndex(TypeIndex idx) { return untagProjectionRef(idx); }
inline const Subtype* subtypeFromTaggedIndex(TypeIndex idx) { return untagSubtypeRef(idx); }

// Lookup-by-parameters translators for the three dedup sets. Each translator
// hashes/equals a param struct against the stored (const T*) entry; on cache
// miss the `ensure` functor moves the caller's Vector / Ref into the
// SegmentedVector-owned object.

struct RecursionGroupLookupKey {
    const Vector<TypeIndex>& types;
};

struct RecursionGroupLookupTranslator {
    static unsigned hash(const RecursionGroupLookupKey& params)
    {
        return computeRecursionGroupHash(params.types.span());
    }
    static bool equal(const RecursionGroup* g, const RecursionGroupLookupKey& params)
    {
        if (!g)
            return false;
        if (g->typeCount() != params.types.size())
            return false;
        for (RecursionGroupCount i = 0; i < g->typeCount(); ++i) {
            if (g->type(i) != params.types[i])
                return false;
        }
        return true;
    }
};

struct ProjectionLookupKey {
    TypeIndex recursionGroup;
    ProjectionIndex projectionIndex;
};

struct ProjectionLookupTranslator {
    static unsigned hash(const ProjectionLookupKey& params)
    {
        return computeProjectionHash(params.recursionGroup, params.projectionIndex);
    }
    static bool equal(const Projection* p, const ProjectionLookupKey& params)
    {
        if (!p)
            return false;
        return p->recursionGroup() == params.recursionGroup
            && p->projectionIndex() == params.projectionIndex;
    }
};

struct SubtypeLookupKey {
    const Vector<TypeIndex>& superTypes;
    const RTT* underlyingRTT;
    bool isFinal;
};

struct SubtypeLookupTranslator {
    static unsigned hash(const SubtypeLookupKey& params)
    {
        return computeSubtypeHash(params.superTypes.span(), std::bit_cast<TypeIndex>(params.underlyingRTT), params.isFinal);
    }
    static bool equal(const Subtype* s, const SubtypeLookupKey& params)
    {
        if (!s)
            return false;
        if (s->supertypeCount() != params.superTypes.size())
            return false;
        for (SupertypeCount i = 0; i < params.superTypes.size(); ++i) {
            if (s->superType(i) != params.superTypes[i])
                return false;
        }
        if (&s->underlyingRTT() != params.underlyingRTT)
            return false;
        if (s->isFinal() != params.isFinal)
            return false;
        return true;
    }
};

} // anonymous namespace

void TypeSectionState::reserveForRecursionGroup(uint32_t typeCount)
{
    m_projectionDedup.reserveInitialCapacity(typeCount);
    m_subtypeDedup.reserveInitialCapacity(typeCount);
}

const RecursionGroup* TypeSectionState::createRecursionGroup(Vector<TypeIndex>&& types)
{
    auto result = m_recursionGroupDedup.ensure<RecursionGroupLookupTranslator>(
        RecursionGroupLookupKey { types },
        [&] -> const RecursionGroup* {
            return &m_recursionGroupStorage.alloc(WTF::move(types));
        });
    return *result.iterator;
}

const Projection* TypeSectionState::createProjection(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
{
    auto result = m_projectionDedup.ensure<ProjectionLookupTranslator>(
        ProjectionLookupKey { recursionGroup, projectionIndex },
        [&] -> const Projection* {
            return &m_projectionStorage.alloc(recursionGroup, projectionIndex);
        });
    return *result.iterator;
}

const Projection* TypeSectionState::createProjectionDirect(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
{
    // Sequential projections (groupIndex, 0..N-1) are unique by construction;
    // skip the lookup-by-params path.
    Projection& projection = m_projectionStorage.alloc(recursionGroup, projectionIndex);
    auto addResult = m_projectionDedup.add(&projection);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return &projection;
}

const Subtype* TypeSectionState::createSubtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal)
{
    auto result = m_subtypeDedup.ensure<SubtypeLookupTranslator>(
        SubtypeLookupKey { superTypes, underlyingRTT.ptr(), isFinal },
        [&] -> const Subtype* {
            return &m_subtypeStorage.alloc(WTF::move(superTypes), WTF::move(underlyingRTT), isFinal);
        });
    return *result.iterator;
}

const Projection* TypeSectionState::createPlaceholderProjection(ProjectionIndex projectionIndex)
{
    auto* projection = createProjection(Projection::PlaceholderGroup, projectionIndex);
    m_placeholders.add(projection);
    return projection;
}

ALWAYS_INLINE Type TypeSectionState::substitute(Type type, TypeIndex projectee)
{
    if (isRefWithTypeIndex(type) && TypeInformation::isPlaceholderRef(type.index)) {
        const Projection* projection = projectionFromTaggedIndex(type.index);
        if (projection->isPlaceholder()) {
            auto* newProjection = createProjection(projectee, projection->projectionIndex());
            TypeKind kind = type.isNullable() ? TypeKind::RefNull : TypeKind::Ref;
            RELEASE_ASSERT(newProjection);
            return Type { kind, TypeInformation::placeholderRefIndex(*newProjection) };
        }
    }
    return type;
}

ALWAYS_INLINE TypeIndex TypeSectionState::substituteParent(TypeIndex parent, TypeIndex projectee)
{
    if (TypeInformation::isPlaceholderRef(parent)) {
        const Projection* projection = projectionFromTaggedIndex(parent);
        if (projection->isPlaceholder()) {
            auto* newProjection = createProjection(projectee, projection->projectionIndex());
            RELEASE_ASSERT(newProjection);
            return TypeInformation::placeholderRefIndex(*newProjection);
        }
    }
    return parent;
}

void TypeSectionState::registerCanonicalRTT(const Subtype& subtype)
{
    if (subtype.rtt())
        return;
    auto rtt = createCanonicalRTT(subtype);
    subtype.setRTT(WTF::move(rtt));
}

void TypeSectionState::registerCanonicalRTT(const Projection& projection)
{
    if (projection.rtt())
        return;
    auto rtt = createCanonicalRTT(projection);
    projection.setRTT(WTF::move(rtt));
}

Ref<const RTT> TypeSectionState::createCanonicalRTT(const Subtype& subtype)
{
    bool isFinalType = subtype.isFinal();
    const RTT* expandedRTT = &subtype.underlyingRTT();
    RTTKind kind = expandedRTT->kind();

    const RTT* superRTT = nullptr;
    if (subtype.supertypeCount() > 0) {
        TypeIndex parent = subtype.firstSuperType();
        if (TypeInformation::isPlaceholderRef(parent))
            superRTT = projectionFromTaggedIndex(parent)->rtt();
        else
            superRTT = std::bit_cast<const RTT*>(parent);
        ASSERT(superRTT);
    }

    RefPtr<RTT> protector;
    switch (kind) {
    case RTTKind::Function: {
        RTTFunctionPayload payload {
            expandedRTT->argumentCount(), expandedRTT->returnCount(),
            expandedRTT->functionPayload().signatureSpan(),
            expandedRTT->argumentsOrResultsIncludeI64(),
            expandedRTT->argumentsOrResultsIncludeV128(),
            expandedRTT->argumentsOrResultsIncludeExnref(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateFunction(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateFunction(isFinalType, WTF::move(payload));
        break;
    }
    case RTTKind::Struct: {
        RTTStructPayload payload {
            FixedVector<StructFieldEntry>(expandedRTT->structPayload().fieldsSpan()),
            expandedRTT->instancePayloadSize(),
            expandedRTT->hasRefFieldTypes(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateStruct(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateStruct(isFinalType, WTF::move(payload));
        break;
    }
    case RTTKind::Array: {
        RTTArrayPayload payload {
            expandedRTT->elementType(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateArray(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateArray(isFinalType, WTF::move(payload));
        break;
    }
    }
    RELEASE_ASSERT(protector);
    return protector.releaseNonNull();
}

Ref<const RTT> TypeSectionState::createCanonicalRTT(const Projection& projection)
{
    ASSERT(!projection.isPlaceholder());
    const RecursionGroup* recursionGroup = std::bit_cast<const RecursionGroup*>(projection.recursionGroup());
    TypeIndex memberIndex = recursionGroup->type(projection.projectionIndex());
    TypeIndex projectee = recursionGroup->index();

    auto rebuildWithSubstitution = [&](const RTT& source) -> Ref<const RTT> {
        switch (source.kind()) {
        case RTTKind::Function: {
            // Provider-based construction: the FixedVector<Type> signature
            // inside RTTFunctionPayload is filled directly from substituted
            // values, skipping the intermediate Vector<Type, 16> + copy the
            // span-based overload would require.
            return TypeInformation::typeDefinitionForFunctionFromProviders(
                source.returnCount(),
                [&](size_t i) { return substitute(source.returnType(i), projectee); },
                source.argumentCount(),
                [&](size_t i) { return substitute(source.argumentType(i), projectee); });
        }
        case RTTKind::Struct: {
            // Same trick for struct fields.
            return TypeInformation::typeDefinitionForStructFromProvider(
                source.fieldCount(),
                [&](size_t i) {
                    FieldType field = source.field(i);
                    StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), projectee));
                    return FieldType { substituted, field.mutability };
                });
        }
        case RTTKind::Array: {
            FieldType field = source.elementType();
            StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), projectee));
            return TypeInformation::typeDefinitionForArray(FieldType { substituted, field.mutability });
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    if (memberIndex & subtypeTagBit) {
        const Subtype* subtype = subtypeFromTaggedIndex(memberIndex);
        if (subtype->hasRecursiveReference()) {
            Vector<TypeIndex> supertypes(subtype->supertypeCount(), [&](size_t i) {
                return substituteParent(subtype->superType(i), projectee);
            });
            Ref<const RTT> newUnderlyingRTT = rebuildWithSubstitution(subtype->underlyingRTT());
            auto* unrolled = createSubtype(WTF::move(supertypes), WTF::move(newUnderlyingRTT), subtype->isFinal());
            RELEASE_ASSERT(unrolled);
            return createCanonicalRTT(*unrolled);
        }
        return createCanonicalRTT(*subtype);
    }
    const RTT* rtt = std::bit_cast<const RTT*>(memberIndex);
    if (rtt->hasRecursiveReference())
        return rebuildWithSubstitution(*rtt);
    return Ref<const RTT> { *rtt };
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
