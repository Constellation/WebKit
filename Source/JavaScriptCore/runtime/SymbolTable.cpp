/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SymbolTable.h"

#include "CodeBlock.h"
#include "DebuggerLocation.h"
#include "JSCJSValueInlines.h"
#include "ResourceExhaustion.h"
#include "ScriptExecutable.h"
#include "TypeProfiler.h"
#include "UnlinkedSymbolTable.h"

#include <wtf/CommaPrinter.h>

namespace JSC {

const ClassInfo SymbolTable::s_info = { "SymbolTable"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(SymbolTable) };

// Delegation methods to UnlinkedSymbolTable

UnlinkedSymbolTable& SymbolTable::unlinkedSymbolTable() const { return m_unlinkedSymbolTable.get(); }

SymbolTable::Map::iterator SymbolTable::find(const ConcurrentJSLocker& locker, UniquedStringImpl* key) { return m_unlinkedSymbolTable->find(locker, key); }
SymbolTable::Map::iterator SymbolTable::find(const GCSafeConcurrentJSLocker& locker, UniquedStringImpl* key) { return m_unlinkedSymbolTable->find(locker, key); }

SymbolTableEntry SymbolTable::get(const ConcurrentJSLocker& locker, UniquedStringImpl* key) { return m_unlinkedSymbolTable->get(locker, key); }
SymbolTableEntry SymbolTable::get(UniquedStringImpl* key) { return m_unlinkedSymbolTable->get(key); }

SymbolTableEntry SymbolTable::inlineGet(const ConcurrentJSLocker& locker, UniquedStringImpl* key) { return m_unlinkedSymbolTable->inlineGet(locker, key); }
SymbolTableEntry SymbolTable::inlineGet(UniquedStringImpl* key) { return m_unlinkedSymbolTable->inlineGet(key); }

SymbolTable::Map::iterator SymbolTable::begin(const ConcurrentJSLocker& locker) { return m_unlinkedSymbolTable->begin(locker); }
SymbolTable::Map::iterator SymbolTable::end(const ConcurrentJSLocker& locker) { return m_unlinkedSymbolTable->end(locker); }
SymbolTable::Map::iterator SymbolTable::end(const GCSafeConcurrentJSLocker& locker) { return m_unlinkedSymbolTable->end(locker); }

size_t SymbolTable::size(const ConcurrentJSLocker& locker) const { return m_unlinkedSymbolTable->size(locker); }
size_t SymbolTable::size() const { return m_unlinkedSymbolTable->size(); }

ScopeOffset SymbolTable::maxScopeOffset() const { return m_unlinkedSymbolTable->maxScopeOffset(); }
void SymbolTable::didUseScopeOffset(ScopeOffset offset) { m_unlinkedSymbolTable->didUseScopeOffset(offset); }
void SymbolTable::didUseVarOffset(VarOffset offset) { m_unlinkedSymbolTable->didUseVarOffset(offset); }
unsigned SymbolTable::scopeSize() const { return m_unlinkedSymbolTable->scopeSize(); }
ScopeOffset SymbolTable::nextScopeOffset() const { return m_unlinkedSymbolTable->nextScopeOffset(); }
ScopeOffset SymbolTable::takeNextScopeOffset(const ConcurrentJSLocker& locker) { return m_unlinkedSymbolTable->takeNextScopeOffset(locker); }
ScopeOffset SymbolTable::takeNextScopeOffset() { return m_unlinkedSymbolTable->takeNextScopeOffset(); }

bool SymbolTable::hasPrivateNames() const { return m_unlinkedSymbolTable->hasPrivateNames(); }
SymbolTable::PrivateNameIteratorRange SymbolTable::privateNames() { return m_unlinkedSymbolTable->privateNames(); }
void SymbolTable::addPrivateName(const RefPtr<UniquedStringImpl>& key, PrivateNameEntry value) { m_unlinkedSymbolTable->addPrivateName(key, value); }
bool SymbolTable::hasPrivateName(const RefPtr<UniquedStringImpl>& key) const { return m_unlinkedSymbolTable->hasPrivateName(key); }

bool SymbolTable::contains(const ConcurrentJSLocker& locker, UniquedStringImpl* key) { return m_unlinkedSymbolTable->contains(locker, key); }
bool SymbolTable::contains(UniquedStringImpl* key) { return m_unlinkedSymbolTable->contains(key); }

bool SymbolTable::usesSloppyEval() const { return m_unlinkedSymbolTable->usesSloppyEval(); }
void SymbolTable::setUsesSloppyEval(bool value) { m_unlinkedSymbolTable->setUsesSloppyEval(value); }

bool SymbolTable::isNestedLexicalScope() const { return m_unlinkedSymbolTable->isNestedLexicalScope(); }
void SymbolTable::markIsNestedLexicalScope() { m_unlinkedSymbolTable->markIsNestedLexicalScope(); }

void SymbolTable::setScopeType(ScopeType type) { m_unlinkedSymbolTable->setScopeType(type); }
SymbolTable::ScopeType SymbolTable::scopeType() const { return m_unlinkedSymbolTable->scopeType(); }

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(SymbolTableEntryFatEntry);

SymbolTableEntry& SymbolTableEntry::copySlow(const SymbolTableEntry& other)
{
    ASSERT(other.isFat());
    FatEntry* newFatEntry = new FatEntry(*other.fatEntry());
    freeFatEntry();
    m_bits = std::bit_cast<intptr_t>(newFatEntry);
    return *this;
}

void SymbolTable::destroy(JSCell* cell)
{
    SymbolTable* thisObject = static_cast<SymbolTable*>(cell);
    thisObject->SymbolTable::~SymbolTable();
}

void SymbolTableEntry::freeFatEntrySlow()
{
    ASSERT(isFat());
    delete fatEntry();
}

void SymbolTableEntry::prepareToWatch()
{
    if (!isWatchable())
        return;
    FatEntry* entry = inflate();
    if (entry->m_watchpoints)
        return;
    entry->m_watchpoints = WatchpointSet::create(ClearWatchpoint);
}

SymbolTableEntry::FatEntry* SymbolTableEntry::inflateSlow()
{
    FatEntry* entry = new FatEntry(m_bits);
    m_bits = std::bit_cast<intptr_t>(entry);
    return entry;
}

SymbolTable::SymbolTable(VM& vm)
    : JSCell(vm, vm.symbolTableStructure.get())
    , m_unlinkedSymbolTable(UnlinkedSymbolTable::create())
{
}

SymbolTable::SymbolTable(VM& vm, Ref<UnlinkedSymbolTable>&& unlinkedSymbolTable)
    : JSCell(vm, vm.symbolTableStructure.get())
    , m_unlinkedSymbolTable(std::move(unlinkedSymbolTable))
{
}

SymbolTable::~SymbolTable() = default;

template<typename Visitor>
void SymbolTable::visitChildrenImpl(JSCell* thisCell, Visitor& visitor)
{
    SymbolTable* thisSymbolTable = jsCast<SymbolTable*>(thisCell);
    ASSERT_GC_OBJECT_INHERITS(thisSymbolTable, info());
    Base::visitChildren(thisSymbolTable, visitor);

    visitor.append(thisSymbolTable->m_arguments);
    visitor.append(thisSymbolTable->m_clonedFrom);

    // Save some memory. This is O(n) to rebuild and we do so on the fly.
    ConcurrentJSLocker locker(thisSymbolTable->m_lock);
    thisSymbolTable->m_localToEntry = nullptr;
}

DEFINE_VISIT_CHILDREN(SymbolTable);

const SymbolTable::LocalToEntryVec& SymbolTable::localToEntry(const ConcurrentJSLocker&)
{
    if (!m_localToEntry) [[unlikely]] {
        unsigned size = 0;
        for (auto& entry : m_unlinkedSymbolTable->map()) {
            VarOffset offset = entry.value.varOffset();
            if (offset.isScope())
                size = std::max(size, offset.scopeOffset().offset() + 1);
        }

        m_localToEntry = makeUnique<LocalToEntryVec>(size, nullptr);
        for (auto& entry : m_unlinkedSymbolTable->map()) {
            VarOffset offset = entry.value.varOffset();
            if (offset.isScope())
                m_localToEntry->at(offset.scopeOffset().offset()) = &entry.value;
        }
    }

    return *m_localToEntry;
}

SymbolTableEntry* SymbolTable::entryFor(const ConcurrentJSLocker& locker, ScopeOffset offset)
{
    auto& toEntryVector = localToEntry(locker);
    if (offset.offset() >= toEntryVector.size())
        return nullptr;
    return toEntryVector[offset.offset()];
}

SymbolTable* SymbolTable::cloneScopePart(VM& vm)
{
    SymbolTable* result = SymbolTable::create(vm, Ref { m_unlinkedSymbolTable });

    if (m_unlinkedSymbolTable->argumentsLength()) {
        auto length = m_unlinkedSymbolTable->argumentsLength();
        ScopedArgumentsTable* arguments = ScopedArgumentsTable::tryCreate(vm, length);
        RELEASE_ASSERT_RESOURCE_AVAILABLE(arguments, MemoryExhaustion, "Crash intentionally because memory is exhausted.");

        for (uint32_t index = 0; index < length; ++index) {
            ScopeOffset offset = m_unlinkedSymbolTable->argumentScopeOffset(index);
            arguments->trySet(vm, index, offset);
        }

        result->m_arguments.set(vm, result, arguments);
    }

    if (m_rareData) {
        result->ensureRareData();

        {
            auto iter = m_rareData->m_uniqueIDMap.begin();
            auto end = m_rareData->m_uniqueIDMap.end();
            for (; iter != end; ++iter)
                result->m_rareData->m_uniqueIDMap.set(iter->key, iter->value);
        }

        {
            auto iter = m_rareData->m_offsetToVariableMap.begin();
            auto end = m_rareData->m_offsetToVariableMap.end();
            for (; iter != end; ++iter)
                result->m_rareData->m_offsetToVariableMap.set(iter->key, iter->value);
        }

        {
            auto iter = m_rareData->m_uniqueTypeSetMap.begin();
            auto end = m_rareData->m_uniqueTypeSetMap.end();
            for (; iter != end; ++iter)
                result->m_rareData->m_uniqueTypeSetMap.set(iter->key, iter->value);
        }
    }
    result->m_clonedFrom.set(vm, result, this);
    return result;
}

void SymbolTable::prepareForTypeProfiling(const ConcurrentJSLocker&)
{
    if (m_rareData)
        return;

    auto& rareData = ensureRareData();

    for (auto iter = m_unlinkedSymbolTable->map().begin(), end = m_unlinkedSymbolTable->map().end(); iter != end; ++iter) {
        rareData.m_uniqueIDMap.set(iter->key, TypeProfilerNeedsUniqueIDGeneration);
        rareData.m_offsetToVariableMap.set(iter->value.varOffset(), iter->key);
    }
}

String SymbolTable::inferredName()
{
    if (!m_rareData)
        return String();
    return m_rareData->m_inferredName;
}

DebuggerLocation SymbolTable::debuggerLocation()
{
    if (!m_rareData)
        return DebuggerLocation();
    return DebuggerLocation(m_rareData->m_debuggerSourceID, m_rareData->m_debuggerLineColumn.line, m_rareData->m_debuggerLineColumn.column);
}

void SymbolTable::collectDebuggerInfo(CodeBlock* codeBlock)
{
    auto& rareData = ensureRareData();
    if (!rareData.m_inferredName.isNull())
        return;
    rareData.m_inferredName = String::fromUTF8(codeBlock->inferredName().span());
    ScriptExecutable* executable = codeBlock->ownerExecutable();
    if (!executable->isHostFunction()) {
        rareData.m_debuggerSourceID = executable->sourceID();
        rareData.m_debuggerLineColumn = { static_cast<unsigned>(executable->firstLine()), executable->startColumn() };
    }
}

GlobalVariableID SymbolTable::uniqueIDForVariable(const ConcurrentJSLocker&, UniquedStringImpl* key, VM& vm)
{
    RELEASE_ASSERT(m_rareData);

    auto iter = m_rareData->m_uniqueIDMap.find(key);
    auto end = m_rareData->m_uniqueIDMap.end();
    if (iter == end)
        return TypeProfilerNoGlobalIDExists;

    GlobalVariableID id = iter->value;
    if (id == TypeProfilerNeedsUniqueIDGeneration) {
        id = vm.typeProfiler()->getNextUniqueVariableID();
        m_rareData->m_uniqueIDMap.set(key, id);
        m_rareData->m_uniqueTypeSetMap.set(key, TypeSet::create()); // Make a new global typeset for this corresponding ID.
    }

    return id;
}

GlobalVariableID SymbolTable::uniqueIDForOffset(const ConcurrentJSLocker& locker, VarOffset offset, VM& vm)
{
    RELEASE_ASSERT(m_rareData);

    auto iter = m_rareData->m_offsetToVariableMap.find(offset);
    auto end = m_rareData->m_offsetToVariableMap.end();
    if (iter == end)
        return TypeProfilerNoGlobalIDExists;

    return uniqueIDForVariable(locker, iter->value.get(), vm);
}

RefPtr<TypeSet> SymbolTable::globalTypeSetForOffset(const ConcurrentJSLocker& locker, VarOffset offset, VM& vm)
{
    RELEASE_ASSERT(m_rareData);

    uniqueIDForOffset(locker, offset, vm); // Lazily create the TypeSet if necessary.

    auto iter = m_rareData->m_offsetToVariableMap.find(offset);
    auto end = m_rareData->m_offsetToVariableMap.end();
    if (iter == end)
        return nullptr;

    return globalTypeSetForVariable(locker, iter->value.get(), vm);
}

RefPtr<TypeSet> SymbolTable::globalTypeSetForVariable(const ConcurrentJSLocker& locker, UniquedStringImpl* key, VM& vm)
{
    RELEASE_ASSERT(m_rareData);

    uniqueIDForVariable(locker, key, vm); // Lazily create the TypeSet if necessary.

    auto iter = m_rareData->m_uniqueTypeSetMap.find(key);
    auto end = m_rareData->m_uniqueTypeSetMap.end();
    if (iter == end)
        return nullptr;

    return iter->value;
}

#if ASSERT_ENABLED
bool SymbolTable::hasScopedWatchpointSet(WatchpointSet* watchpointSet)
{
    for (auto iter = m_unlinkedSymbolTable->map().begin(), end = m_unlinkedSymbolTable->map().end(); iter != end; ++iter) {
        if (!iter->value.varOffset().isScope())
            continue;

        auto* entryWatchpointSet = iter->value.watchpointSet();
        if (entryWatchpointSet && entryWatchpointSet == watchpointSet)
            return true;
    }

    return false;
}
#endif

SymbolTable::SymbolTableRareData& SymbolTable::ensureRareDataSlow()
{
    auto rareData = makeUnique<SymbolTableRareData>();
    WTF::storeStoreFence();
    m_rareData = WTF::move(rareData);
    return *m_rareData;
}

void SymbolTable::dump(PrintStream& out) const
{
    ConcurrentJSLocker locker(m_lock);
    Base::dump(out);

    CommaPrinter comma;
    out.print(" <"_s);
    for (auto& iter : m_unlinkedSymbolTable->map())
        out.print(comma, *iter.key, ": "_s, iter.value.varOffset());
    out.println(">"_s);
}

} // namespace JSC
