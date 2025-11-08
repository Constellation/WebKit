# Lowercase Mnemonic Optimization

## Change Summary

Moved mnemonic lowercase conversion from runtime C++ code to build-time Python code generation for improved performance.

## Before (Runtime Conversion)

### Python Generator
```python
code += f'    {{ "{instr.name}", "{instr.mnemonic}", '  # Uppercase mnemonic in table
```

### C++ Formatter (Runtime)
```cpp
// Convert mnemonic to lowercase (executed every time!)
char lowercaseMnemonic[32];
const char* src = entry->mnemonic;
char* dst = lowercaseMnemonic;
while (*src && (dst - lowercaseMnemonic) < 31) {
    *dst++ = (*src >= 'A' && *src <= 'Z') ? (*src + 32) : *src;
    src++;
}
*dst = '\0';

// Use lowercaseMnemonic everywhere
offset = snprintf(buffer, bufferSize, "   %-9s", lowercaseMnemonic);
```

## After (Build-Time Conversion)

### Python Generator
```python
# Convert mnemonic to lowercase for table (once during generation)
lowercase_mnemonic = instr.mnemonic.lower()
code += f'    {{ "{instr.name}", "{lowercase_mnemonic}", '
```

### C++ Formatter (Runtime)
```cpp
// Mnemonic is already lowercase in table
// No conversion needed - use directly!
offset = snprintf(buffer, bufferSize, "   %-9s", entry->mnemonic);
```

## Benefits

1. **Performance**: Eliminates per-instruction string conversion at runtime
2. **Code Size**: Removes 9 lines of C++ code from hot path
3. **Simplicity**: Cleaner runtime code without conversion logic
4. **Memory**: Eliminates 32-byte temporary buffer allocation per call

## Implementation Details

### Changes to Python Generator (generate_arm64_disassembler.py)

**Line 773-774**: Added lowercase conversion during table generation
```python
# Convert mnemonic to lowercase for table
lowercase_mnemonic = instr.mnemonic.lower()
```

**Line 848-887**: Removed runtime lowercase conversion from formatter template
- Deleted: 9 lines of char-by-char conversion code
- Added: Comment "Mnemonic is already lowercase in table"
- Changed: All references from `lowercaseMnemonic` to `entry->mnemonic`

### Generated Table Changes

**Before**:
```cpp
{ "ADD_64_addsub_ext", "ADD", 0xffe00000U, 0x8b200000U, 10307, 4, 1 },
{ "B_only_condbranch", "B.", 0xff000010U, 0x54000000U, 10548, 2, 0 },
{ "CASAL_C64_comswap", "CASAL", 0xffe0fc00U, 0xc8e0fc00U, 5385, 3, 0 },
```

**After**:
```cpp
{ "ADD_64_addsub_ext", "add", 0xffe00000U, 0x8b200000U, 10307, 4, 1 },
{ "B_only_condbranch", "b.", 0xff000010U, 0x54000000U, 10548, 2, 0 },
{ "CASAL_C64_comswap", "casal", 0xffe0fc00U, 0xc8e0fc00U, 5385, 3, 0 },
```

## Test Results

All existing tests pass with lowercase mnemonics in table:

### Memory Operands ✅
```
ldur     x24, [x24, #80]         ✅ Lowercase mnemonic
stp      fp, lr, [sp, #-16]!     ✅ Lowercase mnemonic
ldr      x0, [x0]                ✅ Lowercase mnemonic
```

### Conditional Branches ✅
```
b.lt     #0    ✅ Lowercase with dot
b.ge     #0    ✅ Lowercase with dot
b.eq     #0    ✅ Lowercase with dot
```

### Atomic Operations ✅
```
casal    x3, x1, [x2]            ✅ Lowercase mnemonic
```

### Extended Register ✅
```
add      x2, x22, w2, uxtw       ✅ Lowercase mnemonic
```

## Performance Impact

**Estimated savings per instruction formatted**:
- Character-by-character conversion: Eliminated
- Temporary buffer allocation: Eliminated (32 bytes)
- String operations: Reduced from O(n) to O(1) where n = mnemonic length

For a typical 8-character mnemonic:
- Before: ~8-16 operations (conversion) + string formatting
- After: String formatting only

**Actual impact**: Minimal overhead removed from hot path. Formatter now has one less operation per call.

## Compatibility

No API changes - `formatInstruction()` signature remains identical. Only internal implementation optimized.

## Files Modified

1. **generate_arm64_disassembler.py**:
   - Line 773-774: Added lowercase conversion in table generation
   - Lines 848-887: Removed runtime conversion from formatter template

2. **A64InstructionTable.cpp** (regenerated):
   - All 4,013 instruction entries now have lowercase mnemonics
   - Formatter code simplified (9 lines removed)

## Status

**Implementation**: ✅ Complete
**Testing**: ✅ All tests pass
**Performance**: ✅ Runtime conversion eliminated
**Integration**: ✅ Ready

Lowercase mnemonics now stored in table and used directly at runtime without conversion overhead.

---

**Date**: November 8, 2025
**Change Type**: Performance optimization
**Impact**: Eliminates per-instruction string conversion, simplifies runtime code
**Files**: generate_arm64_disassembler.py, A64InstructionTable.cpp (regenerated)
