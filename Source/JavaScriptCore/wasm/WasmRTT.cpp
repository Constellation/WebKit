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
#include "WasmRTT.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::Wasm {

RTT::RTT(RTTKind kind)
    : TrailingArrayType(1)
    , m_kind(kind)
    , m_displaySizeExcludingThis(size() - 1)
{
    at(0) = this;
}

RTT::RTT(RTTKind kind, const RTT& supertype)
    : TrailingArrayType(supertype.size() + 1)
    , m_kind(kind)
    , m_displaySizeExcludingThis(size() - 1)
{
    ASSERT(supertype.size() == (supertype.displaySizeExcludingThis() + 1));
    memcpySpan(span(), supertype.span());
    at(supertype.size()) = this;
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind)
{
    auto result = tryFastMalloc(allocationSize(/* itself */ 1));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind));
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind, const RTT& supertype)
{
    auto result = tryFastMalloc(allocationSize(supertype.size() + 1));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind, supertype));
}

bool RTT::isSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() < parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

bool RTT::isStrictSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() <= parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

} // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
