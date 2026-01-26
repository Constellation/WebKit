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

#if ENABLE(ARM64_DISASSEMBLER)

IGNORE_WARNINGS_BEGIN("error=undef")
IGNORE_WARNINGS_BEGIN("undef")
IGNORE_WARNINGS_BEGIN("missing-prototypes")
IGNORE_WARNINGS_BEGIN("cast-qual")
IGNORE_WARNINGS_BEGIN("cast-align")
IGNORE_WARNINGS_BEGIN("sign-compare")
IGNORE_WARNINGS_BEGIN("documentation")
IGNORE_WARNINGS_BEGIN("unused-parameter")
IGNORE_WARNINGS_BEGIN("missing-field-initializers")

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "binja_impl.c.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END

const char* arm64Disassemble(uint32_t* pc, char* buffer, size_t size)
{
    Instruction instr;
    int status = aarch64_decompose(*pc, &instr, (uint64_t)pc);
    if (status == DECODE_STATUS_OK)
        aarch64_disassemble(&instr, buffer, size);
    else
        buffer[0] = '\0';
    return buffer;
}

#endif // ENABLE(ARM64_DISASSEMBLER)
