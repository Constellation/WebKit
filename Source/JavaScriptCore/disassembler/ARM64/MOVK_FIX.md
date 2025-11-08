# MOVK Immediate Formatting Fix

## Issue

MOVK instructions were displaying incorrectly:
- **Actual**: `movk x8, #17332, , lsl #16`
- **Expected**: `movk x8, #0x43b4, lsl #16`

Two problems:
1. Immediate shown as **decimal** (`#17332`) instead of **hex** (`#0x43b4`)
2. **Double comma** before the shift amount

## Root Cause

### Problem 1: "unsigned" Matched "signed"

The parser checked for `'signed' in hover_lower` to detect signed immediates, but this incorrectly matched "**unsigned** immediate" because it contains the substring "signed".

**MOVK hover text**:
```
"Is the 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field."
```

The check `'signed' in hover_lower` returned `True` for "unsigned", causing imm16 to be classified as `IMM_SINT` (31) instead of `IMM_UINT` (30).

### Problem 2: Operand Structure Differences

**MOV/MOVZ** use a **composite operand** (imm16 + hw combined):
```cpp
{ 30, 0, 5, 16, 21, 2 }  // IMM_UINT with field1=imm16 (5-20), field2=hw (21-22)
```

**MOVK** uses **separate operands**:
```cpp
{ 30, 0, 5, 16, 255, 0 }  // IMM_UINT: imm16 only (bits 5-20)
{ 30, 0, 21, 2, 255, 0 }  // IMM_UINT: hw only (bits 21-22)
```

The formatter for standalone `hw` field output `, lsl #16` with a leading comma, but the main formatting loop also adds a comma separator, causing the double comma.

And the standalone `imm16` field used the default unsigned format `#%u` (decimal) instead of hex.

## Solution

### Fix 1: Check "unsigned" Before "signed"

**File**: `generate_arm64_disassembler.py` line 487-491

Changed from:
```python
elif 'signed' in hover_lower or 'offset' in hover_lower:
    return Operand('IMM_SINT', None, primary_field or 'imm', None, is_optional, hover)
return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)
```

To:
```python
elif 'unsigned' in hover_lower:
    return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)
elif 'signed' in hover_lower or 'offset' in hover_lower:
    return Operand('IMM_SINT', None, primary_field or 'imm', None, is_optional, hover)
return Operand('IMM_UINT', None, primary_field or 'imm', None, is_optional, hover)
```

Now `'unsigned' in hover_lower` is checked first, correctly classifying MOVK's imm16 as `IMM_UINT`.

### Fix 2: Handle Separate Operands in Formatter

**File**: `generate_arm64_disassembler.py` lines 1017-1046

Updated the `IMM_UINT` formatter to handle three cases:

**Case 1**: Composite operand (MOV/MOVZ style)
```cpp
if (op.field2_width > 0 && op.field2_start < 32) {
    // Has both imm16 and hw fields
    offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1_val);
    if (field2_val != 0) {
        offset += snprintf(buffer + offset, bufferSize - offset, ", lsl #%u", field2_val * 16);
    }
}
```

**Case 2**: Standalone hw field (MOVK shift amount)
```cpp
else if (op.field1_start == 21 && op.field1_width == 2) {
    // Main loop already adds ", " separator, so just output "lsl #<amount>"
    if (field1_val != 0) {
        offset += snprintf(buffer + offset, bufferSize - offset, "lsl #%u", field1_val * 16);
    }
}
```
**Key change**: Removed the leading `, ` since the main loop already adds it.

**Case 3**: Standalone imm16 field (MOVK immediate)
```cpp
else if (op.field1_start == 5 && op.field1_width == 16) {
    // Format as hex for consistency with MOV/MOVZ
    offset += snprintf(buffer + offset, bufferSize - offset, "#0x%x", field1_val);
}
```
**Key change**: Use hex format `#0x%x` instead of decimal `#%u`.

## Test Results

### Before Fix
```
MOVK x8, #0x43b4, LSL #16
Formatted:    movk     x8, #17332, , lsl #16    ❌
Expected:     movk     x8, #0x43b4, lsl #16
```

### After Fix
```
MOVK x8, #0x43b4, LSL #16
Formatted:    movk     x8, #0x43b4, lsl #16     ✅
Expected:     movk     x8, #0x43b4, lsl #16
```

### All Test Results ✅
- `test_mov_issues.cpp`: ✅ Both MOVZ and MOVK pass
- `test_mov_shifted.cpp`: ✅ All shift amounts (0, 16, 32) pass
- `test_lowercase.cpp`: ✅ Lowercase mnemonics work
- `test_memory_operands.cpp`: ✅ Memory operands still correct

## Impact

This fix applies to:
- **MOVK** (Move Wide with Keep) - All variants
- Any other instructions with "unsigned immediate" in hover text
- All instructions with separate hw shift operands

All other instructions continue to work correctly.

## Files Modified

1. **generate_arm64_disassembler.py**:
   - Lines 487-491: Added `'unsigned'` check before `'signed'` check
   - Lines 1017-1046: Enhanced IMM_UINT formatter for separate operands

2. **A64InstructionTable.cpp** (regenerated):
   - MOVK operands now correctly typed as `IMM_UINT`
   - Formatter handles both composite and separate operand styles

3. **test_mov_shifted.cpp**:
   - Updated test expectations to match ARM syntax (raw immediate + shift)

## Status

**Implementation**: ✅ Complete
**Testing**: ✅ All tests pass
**Regression**: ✅ No issues found
**Integration**: ✅ Ready

MOVK immediates now display correctly with hex format and proper shift syntax!

---

**Date**: November 8, 2025
**Changes**: Fixed "unsigned" vs "signed" detection, added separate operand handling
**Files Modified**: generate_arm64_disassembler.py, A64InstructionTable.cpp, test_mov_shifted.cpp
**Impact**: Correct formatting for MOVK and all instructions with unsigned immediates
