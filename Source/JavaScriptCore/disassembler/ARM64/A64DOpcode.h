/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#include "A64InstructionTable.h"
#include <stdint.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace ARM64Disassembler {

// Compatibility wrapper for the new table-based disassembler
// Maintains the same API as the old A64DOpcode class
class A64DOpcode {
public:
    A64DOpcode(uint32_t* startPC = nullptr, uint32_t* endPC = nullptr)
        : m_startPC(startPC)
        , m_endPC(endPC)
    {
        m_formatBuffer[0] = '\0';
    }

    const char* disassemble(uint32_t* currentPC)
    {
        if (!currentPC) {
            m_formatBuffer[0] = '\0';
            return m_formatBuffer;
        }

        uint32_t opcode = *currentPC;

        // Find instruction in table
        const InstructionEntry* entry = findInstruction(opcode);

        // Format instruction using the new formatter
        formatInstruction(entry, opcode, currentPC, m_startPC, m_endPC,
                         m_formatBuffer, sizeof(m_formatBuffer));

        return m_formatBuffer;
    }

private:
    uint32_t* m_startPC;
    uint32_t* m_endPC;
    char m_formatBuffer[256];
};

}} // namespace JSC::ARM64Disassembler

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
