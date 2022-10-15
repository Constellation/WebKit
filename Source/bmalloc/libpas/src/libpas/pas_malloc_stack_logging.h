/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef PAS_MALLOC_STACK_LOGGING_H
#define PAS_MALLOC_STACK_LOGGING_H

#include "pas_allocation_result.h"
#include "pas_darwin_spi.h"
#include "pas_heap_config.h"
#include "pas_root.h"
#include "pas_utils.h"
#include <malloc/malloc.h>

PAS_BEGIN_EXTERN_C;

enum pas_msl_is_enabled_flag {
    pas_msl_is_enabled_flag_enabled,
    pas_msl_is_enabled_flag_disabled,
    pas_msl_is_enabled_flag_indeterminate,
};
typedef enum pas_msl_is_enabled_flag pas_msl_is_enabled_flag;


extern pas_msl_is_enabled_flag pas_msl_is_enabled_flag_value;
PAS_API bool pas_compute_msl_is_enabled(void);
static PAS_ALWAYS_INLINE bool pas_msl_is_enabled(void)
{
    switch (pas_msl_is_enabled_flag_value) {
    case pas_msl_is_enabled_flag_indeterminate:
        return pas_compute_msl_is_enabled();
    case pas_msl_is_enabled_flag_enabled:
        return true;
    case pas_msl_is_enabled_flag_disabled:
        return false;
    }
    return false;
}

static PAS_ALWAYS_INLINE pas_allocation_result pas_msl_malloc_logging(pas_heap_config_kind kind, size_t size, pas_allocation_result result)
{
    PAS_UNUSED_PARAM(kind);
    if (PAS_UNLIKELY(malloc_logger && pas_msl_is_enabled())) {
        if (result.did_succeed)
            malloc_logger(pas_stack_logging_type_alloc, (uintptr_t)0, (uintptr_t)size, 0, (uintptr_t)result.begin, 0);
    }
    return result;
}

static PAS_ALWAYS_INLINE pas_allocation_result pas_msl_realloc_logging(pas_heap_config_kind kind, void* old_ptr, size_t new_size, pas_allocation_result result)
{
    PAS_UNUSED_PARAM(kind);
    if (PAS_UNLIKELY(malloc_logger && pas_msl_is_enabled())) {
        if (result.did_succeed)
            malloc_logger(pas_stack_logging_type_alloc | pas_stack_logging_type_dealloc, (uintptr_t)0, (uintptr_t)old_ptr, (uintptr_t)new_size, (uintptr_t)result.begin, 0);
    }
    return result;
}

static PAS_ALWAYS_INLINE void pas_msl_free_logging(pas_heap_config_kind kind, void* ptr)
{
    PAS_UNUSED_PARAM(kind);
    if (PAS_UNLIKELY(malloc_logger && pas_msl_is_enabled()))
        malloc_logger(pas_stack_logging_type_dealloc, (uintptr_t)0, (uintptr_t)ptr, 0, 0, 0);
}

PAS_END_EXTERN_C;

#endif /* PAS_MALLOC_STACK_LOGGING_H */
