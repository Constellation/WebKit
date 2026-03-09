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

#include <JavaScriptCore/ConcurrentJSLock.h>
#include <JavaScriptCore/ConstantMode.h>
#include <JavaScriptCore/InferredValue.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/LineColumn.h>
#include <JavaScriptCore/ScopedArgumentsTable.h>
#include <JavaScriptCore/SourceID.h>
#include <JavaScriptCore/TypeLocation.h>
#include <JavaScriptCore/VarOffset.h>
#include <JavaScriptCore/VariableEnvironment.h>
#include <JavaScriptCore/Watchpoint.h>
#include <memory>
#include <wtf/HashTraits.h>
#include <wtf/text/SymbolImpl.h>

namespace JSC {

class CodeBlock;
class SymbolTable;
class UnlinkedSymbolTable;
struct DebuggerLocation;

static ALWAYS_INLINE int missingSymbolMarker() { return std::numeric_limits<int>::max(); }

// The bit twiddling in this class assumes that every register index is a
// reasonably small positive or negative number, and therefore has its high
// four bits all set or all unset.

struct SymbolTableEntry {
    friend class CachedSymbolTableEntry;

private:
    static VarOffset varOffsetFromBits(intptr_t bits)
    {
        VarKind kind;
        intptr_t kindBits = bits & KindBitsMask;
        if (kindBits <= UnwatchableScopeKindBits)
            kind = VarKind::Scope;
        else if (kindBits == StackKindBits)
            kind = VarKind::Stack;
        else
            kind = VarKind::DirectArgument;
        return VarOffset::assemble(kind, static_cast<int>(bits >> FlagBits));
    }

    static ScopeOffset scopeOffsetFromBits(intptr_t bits)
    {
        ASSERT((bits & KindBitsMask) <= UnwatchableScopeKindBits);
        return ScopeOffset(static_cast<int>(bits >> FlagBits));
    }

public:

    // Use the SymbolTableEntry::Fast class, either via implicit cast or by calling
    // getFast(), when you (1) only care about isNull(), getIndex(), and isReadOnly(),
    // and (2) you are in a hot path where you need to minimize the number of times
    // that you branch on isFat() when getting the bits().
    class Fast {
    public:
        Fast()
            : m_bits(SlimFlag)
        {
        }

        ALWAYS_INLINE Fast(const SymbolTableEntry& entry)
            : m_bits(entry.m_bits)
        {
        }

        bool isNull() const
        {
            return !(m_bits & ~SlimFlag);
        }

        VarOffset varOffset() const
        {
            return varOffsetFromBits(m_bits);
        }

        // Asserts if the offset is anything but a scope offset. This structures the assertions
        // in a way that may result in better code, even in release, than doing
        // varOffset().scopeOffset().
        ScopeOffset scopeOffset() const
        {
            return scopeOffsetFromBits(m_bits);
        }

        bool isReadOnly() const
        {
            return m_bits & ReadOnlyFlag;
        }

        bool isDontEnum() const
        {
            return m_bits & DontEnumFlag;
        }

        unsigned getAttributes() const
        {
            unsigned attributes = 0;
            if (isReadOnly())
                attributes |= PropertyAttribute::ReadOnly;
            if (isDontEnum())
                attributes |= PropertyAttribute::DontEnum;
            return attributes;
        }

    private:
        friend struct SymbolTableEntry;
        intptr_t m_bits;
    };

    SymbolTableEntry()
        : m_bits(SlimFlag)
    {
    }

    SymbolTableEntry(VarOffset offset)
        : m_bits(SlimFlag)
    {
        ASSERT(isValidVarOffset(offset));
        pack(offset, true, false, false);
    }

    SymbolTableEntry(VarOffset offset, unsigned attributes)
        : m_bits(SlimFlag)
    {
        ASSERT(isValidVarOffset(offset));
        pack(offset, true, attributes & PropertyAttribute::ReadOnly, attributes & PropertyAttribute::DontEnum);
    }

    bool isNull() const
    {
        return !(m_bits & ~SlimFlag);
    }

    VarOffset varOffset() const
    {
        return varOffsetFromBits(m_bits);
    }

    bool isWatchable() const
    {
        return (m_bits & KindBitsMask) == ScopeKindBits && Options::useJIT();
    }

    // Asserts if the offset is anything but a scope offset. This structures the assertions
    // in a way that may result in better code, even in release, than doing
    // varOffset().scopeOffset().
    ScopeOffset scopeOffset() const
    {
        return scopeOffsetFromBits(m_bits);
    }

    ALWAYS_INLINE Fast getFast() const
    {
        return Fast(*this);
    }

    unsigned getAttributes() const
    {
        return getFast().getAttributes();
    }

    void setReadOnly()
    {
        m_bits |= ReadOnlyFlag;
    }

    bool isReadOnly() const
    {
        return m_bits & ReadOnlyFlag;
    }

    ConstantMode constantMode() const
    {
        return modeForIsConstant(isReadOnly());
    }

    bool isDontEnum() const
    {
        return m_bits & DontEnumFlag;
    }

private:
    static const intptr_t SlimFlag = 0x1;
    static const intptr_t ReadOnlyFlag = 0x2;
    static const intptr_t DontEnumFlag = 0x4;
    static const intptr_t NotNullFlag = 0x8;
    static const intptr_t KindBitsMask = 0x30;
    static const intptr_t ScopeKindBits = 0x00;
    static const intptr_t UnwatchableScopeKindBits = 0x10;
    static const intptr_t StackKindBits = 0x20;
    static const intptr_t DirectArgumentKindBits = 0x30;
    static const intptr_t FlagBits = 6;

    void pack(VarOffset offset, bool isWatchable, bool readOnly, bool dontEnum)
    {
        intptr_t& bitsRef = m_bits;
        bitsRef =
            (static_cast<intptr_t>(offset.rawOffset()) << FlagBits) | NotNullFlag | SlimFlag;
        if (readOnly)
            bitsRef |= ReadOnlyFlag;
        if (dontEnum)
            bitsRef |= DontEnumFlag;
        switch (offset.kind()) {
        case VarKind::Scope:
            if (isWatchable)
                bitsRef |= ScopeKindBits;
            else
                bitsRef |= UnwatchableScopeKindBits;
            break;
        case VarKind::Stack:
            bitsRef |= StackKindBits;
            break;
        case VarKind::DirectArgument:
            bitsRef |= DirectArgumentKindBits;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    static bool isValidVarOffset(VarOffset offset)
    {
        return ((static_cast<intptr_t>(offset.rawOffset()) << FlagBits) >> FlagBits) == static_cast<intptr_t>(offset.rawOffset());
    }

    intptr_t m_bits;
};

struct SymbolTableIndexHashTraits : HashTraits<SymbolTableEntry> {
    static constexpr DestructionMode needsDestruction = DoesNotNeedDestruction;
};

class SymbolTable final : public JSCell {
    friend class CachedSymbolTable;

public:
    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, SymbolTableEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, SymbolTableIndexHashTraits> Map;
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, GlobalVariableID, IdentifierRepHash> UniqueIDMap;
    typedef UncheckedKeyHashMap<RefPtr<UniquedStringImpl>, RefPtr<TypeSet>, IdentifierRepHash> UniqueTypeSetMap;
    typedef UncheckedKeyHashMap<VarOffset, RefPtr<UniquedStringImpl>> OffsetToVariableMap;
    typedef Vector<SymbolTableEntry*> LocalToEntryVec;
    typedef WTF::IteratorRange<typename PrivateNameEnvironment::iterator> PrivateNameIteratorRange;

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.symbolTableSpace();
    }

    // Create a SymbolTable that internally creates its own UnlinkedSymbolTable.
    // Used for runtime-created symbol tables (e.g., JSGlobalObject).
    static SymbolTable* create(VM& vm)
    {
        SymbolTable* symbolTable = new (NotNull, allocateCell<SymbolTable>(vm)) SymbolTable(vm);
        symbolTable->finishCreation(vm);
        return symbolTable;
    }

    // Create a SymbolTable from an existing UnlinkedSymbolTable.
    // Used during CodeBlock linking.
    static SymbolTable* create(VM& vm, Ref<UnlinkedSymbolTable>&& unlinkedSymbolTable)
    {
        SymbolTable* symbolTable = new (NotNull, allocateCell<SymbolTable>(vm)) SymbolTable(vm, std::move(unlinkedSymbolTable));
        symbolTable->finishCreation(vm);
        return symbolTable;
    }

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    UnlinkedSymbolTable& unlinkedSymbolTable() const;

    // You must hold the lock until after you're done with the iterator.
    Map::iterator find(const ConcurrentJSLocker&, UniquedStringImpl*);
    Map::iterator find(const GCSafeConcurrentJSLocker&, UniquedStringImpl*);

    SymbolTableEntry get(const ConcurrentJSLocker&, UniquedStringImpl*);
    SymbolTableEntry get(UniquedStringImpl*);
    SymbolTableEntry inlineGet(const ConcurrentJSLocker&, UniquedStringImpl*);
    SymbolTableEntry inlineGet(UniquedStringImpl*);

    Map::iterator begin(const ConcurrentJSLocker&);
    Map::iterator end(const ConcurrentJSLocker&);
    Map::iterator end(const GCSafeConcurrentJSLocker&);

    size_t size(const ConcurrentJSLocker&) const;
    size_t size() const;

    ScopeOffset maxScopeOffset() const;
    void didUseScopeOffset(ScopeOffset);
    void didUseVarOffset(VarOffset);
    unsigned scopeSize() const;
    ScopeOffset nextScopeOffset() const;
    ScopeOffset takeNextScopeOffset(const ConcurrentJSLocker&);
    ScopeOffset takeNextScopeOffset();

    template<typename Entry>
    void add(const ConcurrentJSLocker& locker, UniquedStringImpl* key, Entry&& entry);

    template<typename Entry>
    void add(UniquedStringImpl* key, Entry&& entry)
    {
        ConcurrentJSLocker locker(m_lock);
        add(locker, key, std::forward<Entry>(entry));
    }

    bool hasPrivateNames() const;
    PrivateNameIteratorRange privateNames();
    void addPrivateName(const RefPtr<UniquedStringImpl>&, PrivateNameEntry);
    bool hasPrivateName(const RefPtr<UniquedStringImpl>&) const;

    template<typename Entry>
    void set(const ConcurrentJSLocker& locker, UniquedStringImpl* key, Entry&& entry);

    template<typename Entry>
    void set(UniquedStringImpl* key, Entry&& entry)
    {
        ConcurrentJSLocker locker(m_lock);
        set(locker, key, std::forward<Entry>(entry));
    }

    bool contains(const ConcurrentJSLocker&, UniquedStringImpl*);
    bool contains(UniquedStringImpl*);

    // The principle behind ScopedArgumentsTable modifications is that we will create one and
    // leave it unlocked - thereby allowing in-place changes - until someone asks for a pointer to
    // the table. Then, we will lock it. Then both our future changes and their future changes
    // will first have to make a copy. This discipline means that usually when we create a
    // ScopedArguments object, we don't have to make a copy of the ScopedArgumentsTable - instead
    // we just take a reference to one that we already have.

    uint32_t argumentsLength() const
    {
        if (!m_arguments)
            return 0;
        return m_arguments->length();
    }

    bool trySetArgumentsLength(VM& vm, uint32_t length)
    {
        if (!m_arguments) [[unlikely]] {
            ScopedArgumentsTable* table = ScopedArgumentsTable::tryCreate(vm, length);
            if (!table) [[unlikely]]
                return false;
            m_arguments.set(vm, this, table);
        } else {
            ScopedArgumentsTable* table = m_arguments->trySetLength(vm, length);
            if (!table) [[unlikely]]
                return false;
            m_arguments.set(vm, this, table);
        }

        return true;
    }

    ScopeOffset argumentOffset(uint32_t i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_arguments);
        return m_arguments->get(i);
    }

    bool trySetArgumentOffset(VM& vm, uint32_t i, ScopeOffset offset)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_arguments);
        auto* maybeCloned = m_arguments->trySet(vm, i, offset);
        if (!maybeCloned)
            return false;
        m_arguments.set(vm, this, maybeCloned);
        return true;
    }

    void prepareToWatch(ScopeOffset);
    WatchpointSet* watchpointSet(ScopeOffset);

    void prepareToWatchScopedArgument(ScopeOffset offset, uint32_t i)
    {
        prepareToWatch(offset);
        if (!m_arguments)
            return;

        WatchpointSet* watchpoints = watchpointSet(offset);
        m_arguments->trySetWatchpointSet(i, watchpoints);
    }

    ScopedArgumentsTable* arguments() const
    {
        if (!m_arguments)
            return nullptr;
        m_arguments->lock();
        return m_arguments.get();
    }

    void setArguments(VM& vm, ScopedArgumentsTable* table)
    {
        m_arguments.set(vm, this, table);
    }

    const LocalToEntryVec& localToEntry(const ConcurrentJSLocker&);
    SymbolTableEntry* entryFor(const ConcurrentJSLocker&, ScopeOffset);

    GlobalVariableID uniqueIDForVariable(const ConcurrentJSLocker&, UniquedStringImpl* key, VM&);
    GlobalVariableID uniqueIDForOffset(const ConcurrentJSLocker&, VarOffset, VM&);
    RefPtr<TypeSet> globalTypeSetForOffset(const ConcurrentJSLocker&, VarOffset, VM&);
    RefPtr<TypeSet> globalTypeSetForVariable(const ConcurrentJSLocker&, UniquedStringImpl* key, VM&);

    bool usesSloppyEval() const;
    void setUsesSloppyEval(bool);

    bool isNestedLexicalScope() const;
    void markIsNestedLexicalScope();

    enum ScopeType {
        VarScope,
        GlobalLexicalScope,
        LexicalScope,
        CatchScope,
        CatchScopeWithSimpleParameter,
        FunctionNameScope
    };
    void setScopeType(ScopeType);
    ScopeType scopeType() const;

    void prepareForTypeProfiling(const ConcurrentJSLocker&);

    String inferredName();
    DebuggerLocation debuggerLocation();
    void collectDebuggerInfo(CodeBlock*);

    InferredValue<JSScope>& singleton() { return m_singleton; }

    void notifyCreation(VM& vm, JSScope* scope, const char* reason)
    {
        m_singleton.notifyWrite(vm, this, scope, reason);
    }

    DECLARE_VISIT_CHILDREN;

    DECLARE_EXPORT_INFO;

#if ASSERT_ENABLED
    bool hasScopedWatchpointSet(WatchpointSet*);
#endif

    void finalizeUnconditionally(VM&, CollectionScope);
    void dump(PrintStream&) const;

    struct SymbolTableRareData {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(SymbolTableRareData);
        UniqueIDMap m_uniqueIDMap;
        OffsetToVariableMap m_offsetToVariableMap;
        UniqueTypeSetMap m_uniqueTypeSetMap;
        String m_inferredName;
        SourceID m_debuggerSourceID { 0 };
        LineColumn m_debuggerLineColumn;
    };

private:
    JS_EXPORT_PRIVATE SymbolTable(VM&);
    JS_EXPORT_PRIVATE SymbolTable(VM&, Ref<UnlinkedSymbolTable>&&);
    ~SymbolTable();
    SymbolTableRareData& ensureRareData()
    {
        if (m_rareData) [[likely]]
            return *m_rareData;
        return ensureRareDataSlow();
    }

    DECLARE_DEFAULT_FINISH_CREATION;
    JS_EXPORT_PRIVATE SymbolTableRareData& ensureRareDataSlow();

    Ref<UnlinkedSymbolTable> m_unlinkedSymbolTable;
public:
    mutable ConcurrentJSLock m_lock;

private:
    std::unique_ptr<SymbolTableRareData> m_rareData;

    WriteBarrier<ScopedArgumentsTable> m_arguments;
    InferredValue<JSScope> m_singleton;

    Vector<RefPtr<WatchpointSet>> m_watchpointSets;

    std::unique_ptr<LocalToEntryVec> m_localToEntry;
};

} // namespace JSC
