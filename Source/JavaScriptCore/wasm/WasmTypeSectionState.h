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

#pragma once

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmTypeDefinition.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/SegmentedVector.h>

namespace JSC { namespace Wasm {

// Parser-local owner and deduplicator of Subtype / Projection / RecursionGroup
// objects for one wasm type section. Lives on SectionParser for the duration
// of parseType(); destroyed afterwards along with every object it owns.
//
// Objects are stored by value in SegmentedVector<> so pointers remain stable
// (they are bit-cast to TypeIndex via tagAsSubtypeRef / tagAsProjectionRef and
// carried through tagged encodings). Dedup sets hold raw pointers with a
// structural hash/equal that dereferences the pointee.
//
// None of the three types is refcounted: TypeSectionState is the sole owner,
// and its lifetime strictly encloses every pointer to its contents.
class TypeSectionState {
    WTF_MAKE_NONCOPYABLE(TypeSectionState);
    WTF_MAKE_NONMOVABLE(TypeSectionState);
public:
    TypeSectionState() = default;
    ~TypeSectionState() = default;

    // Create + dedup within this section. Pointer identity is stable for the
    // state's lifetime; callers may keep raw pointers as long as the state
    // outlives them.
    //
    // createSubtype / createRecursionGroup take Vector<TypeIndex>&& so the
    // caller's locally-built Vector is moved into the new object on a cache
    // miss (or dropped on a hit) -- no copy either way.
    const Subtype* createSubtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal);
    const Projection* createProjection(TypeIndex recursionGroup, ProjectionIndex);
    const Projection* createProjectionDirect(TypeIndex recursionGroup, ProjectionIndex);
    void reserveForRecursionGroup(uint32_t typeCount);

    const RecursionGroup* createRecursionGroup(Vector<TypeIndex>&& types);
    const Projection* createPlaceholderProjection(ProjectionIndex);

    // Intra-rec-group reference substitution. Placeholder Projections in
    // `type` / `parent` (tagged with the placeholder bit) get rewritten to
    // real Projection refs into the recursion group named by `projectee`.
    Type substitute(Type, TypeIndex projectee);
    TypeIndex substituteParent(TypeIndex parent, TypeIndex projectee);

    // Lazily build the candidate canonical RTT for a parser-local Subtype or
    // Projection and cache it via setRTT.
    void registerCanonicalRTT(const Subtype&);
    void registerCanonicalRTT(const Projection&);

private:
    Ref<const RTT> createCanonicalRTT(const Subtype&);
    Ref<const RTT> createCanonicalRTT(const Projection&);

    SegmentedVector<Subtype, 64> m_subtypeStorage;
    SegmentedVector<Projection, 64> m_projectionStorage;
    SegmentedVector<RecursionGroup, 4> m_recursionGroupStorage;

    UncheckedKeyHashSet<const Subtype*, SubtypeHash> m_subtypeDedup;
    UncheckedKeyHashSet<const Projection*, ProjectionHash> m_projectionDedup;
    UncheckedKeyHashSet<const RecursionGroup*, RecursionGroupHash> m_recursionGroupDedup;
    UncheckedKeyHashSet<const Projection*> m_placeholders;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
