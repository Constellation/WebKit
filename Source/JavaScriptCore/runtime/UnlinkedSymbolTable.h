/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ConcurrentJSLock.h"
#include "ScopeOffset.h"
#include "SymbolTable.h"
#include <wtf/FixedVector.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class UnlinkedSymbolTable final : public ThreadSafeRefCounted<UnlinkedSymbolTable> {

    friend class CachedUnlinkedSymbolTable;

public:
    using Map = UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, SymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, SymbolTableIndexHashTraits>;

    using ScopeType = SymbolTable::ScopeType;

    using PrivateNameIteratorRange = WTF::IteratorRange<typename PrivateNameEnvironment::iterator>;

    static Ref<UnlinkedSymbolTable> create()
    {
        return adoptRef(*new UnlinkedSymbolTable());
    }

    // Map access
    Map::iterator find(const AbstractLocker&, UniquedStringImpl* key) { return m_map.find(key); }

    SymbolTableEntry get(const AbstractLocker&, UniquedStringImpl* key) { return m_map.get(key); }

    SymbolTableEntry get(UniquedStringImpl* key)
    {
        ConcurrentJSLocker locker(m_lock);
        return get(locker, key);
    }

    SymbolTableEntry inlineGet(const AbstractLocker&, UniquedStringImpl* key) { return m_map.inlineGet(key); }

    SymbolTableEntry inlineGet(UniquedStringImpl* key)
    {
        ConcurrentJSLocker locker(m_lock);
        return inlineGet(locker, key);
    }

    bool contains(const AbstractLocker&, UniquedStringImpl* key) { return m_map.contains(key); }

    bool contains(UniquedStringImpl* key)
    {
        ConcurrentJSLocker locker(m_lock);
        return contains(locker, key);
    }

    Map::iterator begin(const AbstractLocker&) { return m_map.begin(); }
    Map::iterator end(const AbstractLocker&) { return m_map.end(); }

    size_t size(const AbstractLocker&) const { return m_map.size(); }

    size_t size() const
    {
        ConcurrentJSLocker locker(m_lock);
        return m_map.size();
    }

    template<typename Entry>
    void add(const AbstractLocker&, UniquedStringImpl* key, Entry&& entry)
    {
        didUseVarOffset(entry.varOffset());
        Map::AddResult result = m_map.add(key, std::forward<Entry>(entry));
        ASSERT_UNUSED(result, result.isNewEntry);
    }

    template<typename Entry>
    void add(UniquedStringImpl* key, Entry&& entry)
    {
        ConcurrentJSLocker locker(m_lock);
        add(locker, key, std::forward<Entry>(entry));
    }

    template<typename Entry>
    void set(const AbstractLocker&, UniquedStringImpl* key, Entry&& entry)
    {
        didUseVarOffset(entry.varOffset());
        m_map.set(key, std::forward<Entry>(entry));
    }

    template<typename Entry>
    void set(UniquedStringImpl* key, Entry&& entry)
    {
        ConcurrentJSLocker locker(m_lock);
        set(locker, key, std::forward<Entry>(entry));
    }

    // Overloads that accept NoLockingNecessaryTag (for use by BytecodeGeneratorification)
    template<typename Entry>
    void set(const NoLockingNecessaryTag&, UniquedStringImpl* key, Entry&& entry)
    {
        didUseVarOffset(entry.varOffset());
        m_map.set(key, std::forward<Entry>(entry));
    }

    // Scope metadata
    ScopeOffset maxScopeOffset() const { return m_maxScopeOffset; }

    void setMaxScopeOffset(ScopeOffset offset) { m_maxScopeOffset = offset; }

    void didUseScopeOffset(ScopeOffset offset)
    {
        if (!m_maxScopeOffset || m_maxScopeOffset < offset)
            m_maxScopeOffset = offset;
    }

    void didUseVarOffset(VarOffset offset)
    {
        if (offset.isScope())
            didUseScopeOffset(offset.scopeOffset());
    }

    unsigned scopeSize() const
    {
        ScopeOffset maxScopeOffset = this->maxScopeOffset();
        unsigned fastResult = maxScopeOffset.offsetUnchecked() + 1;
        ASSERT(fastResult == (!maxScopeOffset ? 0 : maxScopeOffset.offset() + 1));
        return fastResult;
    }

    ScopeOffset nextScopeOffset() const { return ScopeOffset(scopeSize()); }

    ScopeOffset takeNextScopeOffset(const AbstractLocker&)
    {
        ScopeOffset result = nextScopeOffset();
        m_maxScopeOffset = result;
        return result;
    }

    ScopeOffset takeNextScopeOffset(const NoLockingNecessaryTag&)
    {
        ScopeOffset result = nextScopeOffset();
        m_maxScopeOffset = result;
        return result;
    }

    ScopeOffset takeNextScopeOffset()
    {
        ConcurrentJSLocker locker(m_lock);
        return takeNextScopeOffset(locker);
    }

    bool usesSloppyEval() const { return m_usesSloppyEval; }
    void setUsesSloppyEval(bool usesSloppyEval) { m_usesSloppyEval = usesSloppyEval; }

    bool isNestedLexicalScope() const { return m_nestedLexicalScope; }
    void markIsNestedLexicalScope() { ASSERT(scopeType() == SymbolTable::LexicalScope); m_nestedLexicalScope = true; }

    ScopeType scopeType() const { return static_cast<ScopeType>(m_scopeType); }
    void setScopeType(ScopeType type) { m_scopeType = type; }

    // Private names
    bool hasPrivateNames() const { return m_privateNames.size(); }

    PrivateNameIteratorRange privateNames()
    {
        ASSERT(hasPrivateNames());
        return makeIteratorRange(m_privateNames.begin(), m_privateNames.end());
    }

    void addPrivateName(const RefPtr<UniquedStringImpl>& key, PrivateNameEntry value)
    {
        ASSERT(key && !key->isSymbol());
        ASSERT(m_privateNames.find(key) == m_privateNames.end());
        m_privateNames.add(key, value);
    }

    bool hasPrivateName(const RefPtr<UniquedStringImpl>& key) const
    {
        return m_privateNames.contains(key);
    }

    const PrivateNameEnvironment& privateNameEnvironment() const { return m_privateNames; }
    PrivateNameEnvironment& privateNameEnvironment() { return m_privateNames; }

    // Arguments data
    uint32_t argumentsLength() const { return m_argumentsLength; }

    bool trySetArgumentsLength(uint32_t length)
    {
        m_argumentsLength = length;
        m_argumentScopeOffsets = FixedVector<ScopeOffset>(length);
        return true;
    }

    ScopeOffset argumentScopeOffset(uint32_t index) const
    {
        ASSERT(index < m_argumentsLength);
        return m_argumentScopeOffsets[index];
    }

    void setArgumentScopeOffset(uint32_t index, ScopeOffset offset)
    {
        ASSERT(index < m_argumentsLength);
        m_argumentScopeOffsets[index] = offset;
    }

    Map& map() { return m_map; }
    const Map& map() const { return m_map; }

    mutable ConcurrentJSLock m_lock;

private:
    UnlinkedSymbolTable()
        : m_usesSloppyEval(false)
        , m_nestedLexicalScope(false)
        , m_scopeType(SymbolTable::VarScope)
    {
    }

    Map m_map;
    ScopeOffset m_maxScopeOffset;
    unsigned m_usesSloppyEval : 1;
    unsigned m_nestedLexicalScope : 1;
    unsigned m_scopeType : 3;
    uint32_t m_argumentsLength { 0 };
    FixedVector<ScopeOffset> m_argumentScopeOffsets;
    PrivateNameEnvironment m_privateNames;
};

} // namespace JSC
