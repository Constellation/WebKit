# Register Alias Feature: fp and lr

## Overview

Added standard ARM64 register aliases for better readability:
- **x29** → **fp** (frame pointer)
- **x30** → **lr** (link register)

## Implementation

### Modified Files

**`generate_arm64_disassembler.py`** (Lines 714-762)

Added special case handling in three register type formatters:

1. **case 0: REG_GPR_X** - Standard 64-bit GP registers
2. **case 3: REG_GPR_XSP** - GP registers or stack pointer
3. **case 5: REG_GPR_XZR** - GP registers or zero register

### Code Changes

```cpp
case 0: // REG_GPR_X
    if (field1_val == 29)
        offset += snprintf(buffer + offset, bufferSize - offset, "fp");
    else if (field1_val == 30)
        offset += snprintf(buffer + offset, bufferSize - offset, "lr");
    else
        offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field1_val);
    break;
```

Same logic applied to cases 3 and 5 for consistency across all 64-bit GP register contexts.

## Examples

### Before
```
0xa9bf7bfd:    STP       x29, x30, [sp, #-16]!
0xa8c17bfd:    LDP       x29, x30, [sp], #16
0x910003fd:    MOV       x29, sp
0xd65f03c0:    RET       x30
```

### After
```
0xa9bf7bfd:    STP       fp, lr, [sp, #-16]!    ✅
0xa8c17bfd:    LDP       fp, lr, [sp], #16      ✅
0x910003fd:    MOV       fp, sp                 ✅
0xd65f03c0:    RET       lr                     ✅
```

## Benefits

1. **Improved Readability**
   - Immediately recognizable function prologue/epilogue patterns
   - Standard ARM64 assembly convention
   - Matches other ARM64 disassemblers (objdump, lldb, etc.)

2. **Better Debugging**
   - Frame pointer usage is obvious
   - Link register operations clear
   - Easier to identify function structure

3. **Convention Compliance**
   - ARM Architecture Reference Manual uses these aliases
   - Matches compiler-generated assembly listings
   - Consistent with ARM64 ecosystem

## Common Patterns

### Function Prologue
```asm
stp     fp, lr, [sp, #-16]!    ; Save frame pointer and return address
mov     fp, sp                  ; Set up new frame pointer
```

### Function Epilogue
```asm
ldp     fp, lr, [sp], #16      ; Restore frame pointer and return address
ret     lr                      ; Return via link register
```

### Register Usage
- **fp (x29)**: Points to current stack frame
- **lr (x30)**: Holds return address for function calls

## Implementation Details

### Register Type Coverage

| Register Type | Aliases Applied | Description |
|--------------|----------------|-------------|
| REG_GPR_X (0) | ✅ fp, lr | Standard 64-bit registers |
| REG_GPR_W (1) | ❌ | 32-bit registers (w29, w30) |
| REG_GPR_XSP (3) | ✅ fp, lr | With SP variant |
| REG_GPR_XZR (5) | ✅ fp, lr | With XZR variant |

Note: 32-bit variants (w29, w30) do not use aliases as they're rarely used as frame/link registers.

### Register Number Mapping
- **29** → fp (frame pointer)
- **30** → lr (link register)
- **31** → sp (stack pointer) or xzr/wzr depending on context

## Testing

### Test Cases
```cpp
0xa9bf7bfd  // stp fp, lr, [sp, #-16]!
0xa8c17bfd  // ldp fp, lr, [sp], #16
0x910003fd  // mov fp, sp
0xd65f03c0  // ret lr
```

### Verification
```bash
# Check generated code
grep -A 5 "case 0: // REG_GPR_X" A64InstructionTable.cpp

# Verify output
clang++ test_fp_lr_simple.cpp && ./test_fp_lr_simple
```

## Compatibility

### Existing Code
- ✅ No breaking changes to API
- ✅ All instruction decoding unchanged
- ✅ Only affects display formatting

### Standards Compliance
- ✅ ARM Architecture Reference Manual conventions
- ✅ Matches GCC/Clang assembly output
- ✅ Compatible with lldb/gdb display

## Status

**Implementation**: ✅ Complete
**Testing**: ✅ Verified
**Documentation**: ✅ Complete
**Integration**: ✅ Ready

All 64-bit GP register contexts now properly display:
- x29 as **fp**
- x30 as **lr**

---

**Date**: November 8, 2025
**Change**: Register alias formatting
**Files Modified**: generate_arm64_disassembler.py, A64InstructionTable.cpp
**Impact**: Improved readability, ARM64 convention compliance
