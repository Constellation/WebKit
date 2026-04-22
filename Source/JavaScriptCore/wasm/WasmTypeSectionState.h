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

namespace JSC { namespace Wasm {

// Parser-local owner and deduplicator of Subtype / Projection / RecursionGroup
// objects for one wasm type section. Lives on SectionParser for the duration
// of parseType(); destroyed afterwards along with every object it owns.
//
// This replaces the former TypeInformation::m_subtypes / m_projections /
// m_recursionGroups / m_placeholders global tables. Those tables only ever
// deduplicated parser-internal scaffolding; the authoritative canonical
// identity lives in TypeInformation::m_canonicalRecursionGroups (RTT-level).
// Making these objects parser-local removes the global contention on
// TypeInformation::m_lock during parse.
//
// Pointer identity is preserved: Subtype / Projection / RecursionGroup
// pointers are bit-cast into TypeIndex and carried through tagged encodings.
// The Ref-based dedup sets keep each object alive for the state's lifetime;
// pointers remain stable until the state is destroyed.
class TypeSectionState {
    WTF_MAKE_NONCOPYABLE(TypeSectionState);
    WTF_MAKE_NONMOVABLE(TypeSectionState);
public:
    TypeSectionState() = default;
    ~TypeSectionState() = default;

    // Create + dedup within this section. Pointer identity is stable for the
    // state's lifetime; callers may keep raw pointers as long as the state
    // outlives them.
    RefPtr<Subtype> createSubtype(const Vector<TypeIndex>& superTypes, Ref<const RTT> underlyingRTT, bool isFinal);
    RefPtr<Projection> createProjection(TypeIndex recursionGroup, ProjectionIndex);
    RefPtr<Projection> createProjectionDirect(TypeIndex recursionGroup, ProjectionIndex);
    void reserveForRecursionGroup(uint32_t typeCount);

    RefPtr<RecursionGroup> createRecursionGroup(const Vector<TypeIndex>& types);
    RefPtr<Projection> createPlaceholderProjection(ProjectionIndex);

    // Intra-rec-group reference substitution. Placeholder Projections in
    // `type` / `parent` (tagged with the placeholder bit) get rewritten to
    // real Projection refs into the recursion group named by `projectee`.
    Type substitute(Type, TypeIndex projectee);
    TypeIndex substituteParent(TypeIndex parent, TypeIndex projectee);

    // Lazily build the candidate canonical RTT for a parser-local Subtype or
    // Projection and cache it via setRTT. Replaces the former
    // TypeInformation::registerCanonicalRTTForSubtype/Projection path.
    void registerCanonicalRTT(const Subtype&);
    void registerCanonicalRTT(const Projection&);

private:
    Ref<const RTT> createCanonicalRTT(const Subtype&);
    Ref<const RTT> createCanonicalRTT(const Projection&);

    UncheckedKeyHashSet<SubtypeHash> m_subtypes;
    UncheckedKeyHashSet<ProjectionHash> m_projections;
    UncheckedKeyHashSet<RecursionGroupHash> m_recursionGroups;
    UncheckedKeyHashSet<RefPtr<Projection>> m_placeholders;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
