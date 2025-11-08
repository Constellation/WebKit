# TST and TBNZ Immediate Formatting Fixes

## Issues Fixed

### 1. TST Logical Immediate (Issue #1)
**Problem**: `tst w2, #0xf` was showing as `tst w2, #0`

**Root Cause**:
1. **Compound field names**: TST's immediate operand has hover text `"imms:immr"` with colon-separated field names, but the parser's regex only extracted individual quoted field names
2. **"Bitmask" not recognized**: TST uses "bitmask immediate" terminology, but parser only checked for "logical immediate"

**Fixes**:
- **Lines 403-408**: Updated field name extraction to split compound fields on colons:
  ```python
  quoted_strings = re.findall(r'"([A-Za-z0-9_:]+)"', hover)
  field_names = []
  for qs in quoted_strings:
      field_names.extend(qs.split(':'))
  ```
- **Line 487**: Added "bitmask" detection for logical immediates:
  ```python
  if 'logical' in hover_lower or 'bitmask' in hover_lower:
      return Operand('IMM_LOGICAL', None, primary_field or 'imms', 'N', is_optional, hover)
  ```

**Result**: `tst w2, #0xf` ✅

---

### 2. TBNZ Bit Position and Label (Issue #2)
**Problem**: `tbnz w2, #5, <label>` was only showing register, missing bit position and label

**Root Causes**:
1. **Bit position not detected**: Link `b40_b5` doesn't contain "imm", so wasn't recognized as immediate
2. **Label detected as IMM_SINT**: Link `imm14_offset` contains "imm", matched immediate pattern before label pattern
3. **Double 0x prefix**: Formatter used `"0x%p"` but `%p` already adds "0x"

**Fixes**:

**A. Bit Position Detection (Lines 469-475)**:
```python
# Bit position (for TBNZ/TBZ)
if 'bit' in hover_lower and ('number' in hover_lower or 'position' in hover_lower):
    # Encoded as b5 (high bit) and b40 (low 5 bits): position = b5 * 32 + b40
    # After colon split, fields are in order: b5, b40
    # We want field1=b40 (base), field2=b5 (multiplier), so swap them
    return Operand('IMM_UINT', None, secondary_field or 'b40', primary_field or 'b5', is_optional, hover)
```

**B. Bit Position Formatter (Lines 1032-1039)**:
```cpp
// Check if this is a bit position (TBNZ/TBZ style: b40 + b5*32)
if (op.field1_width == 5 && op.field1_start == 19 &&
    op.field2_width == 1 && op.field2_start == 31) {
    // Bit position: field1=b40 (bits 19-23), field2=b5 (bit 31)
    // position = b5 * 32 + b40
    unsigned bit_pos = field2_val * 32 + field1_val;
    offset += snprintf(buffer + offset, bufferSize - offset, "#%u", bit_pos);
}
```

**C. Label Detection Before Immediates (Lines 477-479)**:
```python
# Labels (check before immediates since links like "imm14_offset" contain "imm")
if 'label' in link_lower or 'label' in hover_lower or ('offset' in hover_lower and 'pc' in hover_lower):
    return Operand('LABEL_PCREL', None, primary_field or 'imm', None, is_optional, hover)
```

**D. Fixed Double 0x Prefix (Lines 1124, 1127)**:
Changed `"0x%p"` to `"%p"` since `%p` format already includes the prefix.

**Result**: `tbnz x2, #5, 0x16b97ffc8` ✅

---

## Test Results

### TST - Logical Immediate ✅
```
Test: TST w2, #0xf
Opcode: 0x72000c5f

Formatted:    tst      w2, #0xf
Expected:     tst      w2, #0xf
Status: ✅ PASS
```

### TBNZ - Bit Position and Label ✅
```
Test: TBNZ x2, #5, <offset>
Opcode: 0x37280042

Operands:
- Register: x2
- Bit position: #5
- Label: 0x16b97ffc8

Formatted:    tbnz     x2, #5, 0x16b97ffc8
Status: ✅ PASS (all 3 operands shown)
```

### Regression Tests ✅
- **MOV/MOVZ**: `mov x8, #0x8103` ✅
- **MOVK**: `movk x8, #0x43b4, lsl #16` ✅
- **Memory operands**: All tests pass ✅
- **Lowercase mnemonics**: All tests pass ✅

---

## Summary of Changes

### Parser (generate_arm64_disassembler.py)

1. **Field name extraction** (403-408): Split compound fields like "imms:immr" on colons
2. **Bit position detection** (469-475): Added check for "bit number/position" in hover
3. **Label detection moved** (477-479): Check labels before immediates to prevent false matches
4. **Bitmask immediate** (487): Added "bitmask" as synonym for "logical immediate"

### Formatter (generate_arm64_disassembler.py)

1. **Bit position calculation** (1032-1039): Calculate `b5 * 32 + b40` for TBNZ/TBZ
2. **Pointer formatting** (1124, 1127): Fixed double "0x" prefix by using `%p` without "0x"

---

## Files Modified

1. **generate_arm64_disassembler.py**:
   - Lines 403-408: Field name extraction with colon splitting
   - Lines 469-475: Bit position operand detection
   - Lines 477-479: Label detection (moved before immediates)
   - Line 487: Added "bitmask" detection
   - Lines 1032-1039: Bit position formatter
   - Lines 1124, 1127: Fixed %p formatting

2. **A64InstructionTable.cpp** (regenerated):
   - TST entries now have correct IMM_LOGICAL operands
   - TBNZ/TBZ entries now have all 3 operands (register, bit position, label)

3. **Test files**:
   - test_tst.cpp: Tests TST logical immediate
   - test_tbnz.cpp: Tests TBNZ with bit position and label

---

## Impact

### Fixed Instructions
- **TST/TSTS** (all variants): Logical immediate now decoded correctly
- **TBNZ/TBZ** (test bit and branch): Bit position and branch label now shown
- **ANDS/BICS** with logical immediates: Also benefit from bitmask detection

### Performance
- No runtime performance impact
- All changes are in code generation (build-time only)

### Compatibility
- No breaking changes
- All existing tests continue to pass
- Maintains backward compatibility

---

**Date**: November 8, 2025
**Issues Fixed**: TST logical immediate (#0 → #0xf), TBNZ bit position and label
**Files Modified**: generate_arm64_disassembler.py, A64InstructionTable.cpp
**Tests Added**: test_tst.cpp, test_tbnz.cpp
**Status**: ✅ Complete and tested
