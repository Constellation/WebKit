# LDRB Register Offset Formatting Fix

## Issue

LDRB with register offset and extend was formatting incorrectly:
- **Actual**: `ldrb w2, [x22, #-12]` (treated as immediate offset)
- **Expected**: `ldrb w2, [x22, w20, uxtw]` (register offset with extend)

## Root Cause

The MEMORY_OFFSET formatter (type=61) treated field2 as an immediate value, sign-extending it to produce an offset. For LDRB with register offset:
- `field2` contains Rm (register number 20) at bits 16-20
- The formatter sign-extended 20 as a 5-bit value → -12
- Result: `[x22, #-12]` instead of `[x22, w20, uxtw]`

### Why This Happened

Load/store register offset instructions have a complex addressing mode:
```
[<base>, <offset_reg>, <extend> {#<shift>}]
```

The parser created a single MEMORY_OFFSET operand with:
- `field1` = Rn (base register, bits 5-9)
- `field2` = Rm (offset register, bits 16-20)

But the formatter didn't distinguish between:
- **Immediate offset**: field2 is an immediate value to sign-extend
- **Register offset**: field2 is a register number, needs extend type from opcode

## Solution

Enhanced the MEMORY_OFFSET formatter to detect register offset addressing and extract the extend information from the opcode.

### Detection Logic (Lines 1174)

```cpp
// Check if field2 is a register offset (load/store register offset addressing)
// Register offset: field2 is Rm at bits 16-20 (width=5)
// Immediate offset: field2 is various immediate fields
if (op.field2_width == 5 && op.field2_start == 16) {
    // This is register offset addressing
```

When `field2` is 5 bits wide at bit position 16, it's the Rm register (offset register), not an immediate.

### Extend Type Extraction (Lines 1177-1178)

```cpp
// Extract extend type from bits 13-15 (option field)
uint32_t option = extractBits(opcode, 13, 3);
uint32_t shift = extractBits(opcode, 12, 1);  // S bit
```

Register offset instructions encode:
- **Bits 13-15** (option): Extend type
  - 0b010 = UXTW (zero-extend word)
  - 0b011 = LSL (logical shift left)
  - 0b110 = SXTW (sign-extend word)
  - 0b111 = SXTX (sign-extend doubleword)
- **Bit 12** (S): Shift enable (shift by log2(size))

### Register Width Detection (Lines 1180-1181)

```cpp
// Determine if offset register is W or X based on option<0>
bool use_w_reg = (option & 1) == 0;
```

- `option[0] = 0` → Use W register (32-bit)
- `option[0] = 1` → Use X register (64-bit)

### Formatting (Lines 1183-1201)

```cpp
offset += snprintf(buffer + offset, bufferSize - offset, ", ");
if (use_w_reg)
    offset += snprintf(buffer + offset, bufferSize - offset, "w%u", field2_val);
else
    offset += snprintf(buffer + offset, bufferSize - offset, "x%u", field2_val);

// Format extend type
const char* extend_names[] = {
    "uxtb", "uxth", "uxtw", "lsl",  // option 0-3
    "sxtb", "sxth", "sxtw", "sxtx"  // option 4-7
};
offset += snprintf(buffer + offset, bufferSize - offset, ", %s", extend_names[option & 0x7]);

// Add shift if present (S=1 means shift by log2(size))
if (shift) {
    uint32_t size = extractBits(opcode, 30, 2);
    offset += snprintf(buffer + offset, bufferSize - offset, " #%u", size);
}
```

## Test Results

### LDRB with Register Offset ✅

```
Test: LDRB w2, [x22, w20, UXTW]
Opcode: 0x38744ac2

Fields:
- Rt: 2 (w2)
- Rn: 22 (x22)
- Rm: 20 (w20)
- option: 2 (UXTW)
- S: 0 (no shift)

Before: ldrb     w2, [x22, #-12]      ❌
After:  ldrb     w2, [x22, w20, uxtw] ✅
```

### Regression Tests ✅

- **MOV/MOVK**: All pass ✅
- **TST**: Logical immediate works ✅
- **Memory operands**: All variants pass ✅
- **TBNZ**: Bit position and label work ✅

## Impact

### Fixed Instructions

All load/store instructions with register offset addressing:
- **Byte**: LDRB, LDRSB, STRB
- **Halfword**: LDRH, LDRSH, STRH
- **Word**: LDR (32-bit), LDRSW, STR (32-bit)
- **Doubleword**: LDR (64-bit), STR (64-bit)
- **FP/SIMD**: LDR/STR variants (B, H, S, D, Q)

All variants with extend types:
- **UXTW**: Zero-extend word (W register)
- **SXTW**: Sign-extend word (W register)
- **LSL**: Logical shift left (X register)
- **SXTX**: Sign-extend doubleword (X register)

### Compatibility

- No breaking changes
- Immediate offset addressing continues to work
- All existing tests pass
- Only affects display format (no functional changes)

## Files Modified

**generate_arm64_disassembler.py** (Lines 1171-1202):
- Added detection for register offset (field2 width=5, start=16)
- Extract extend type from opcode bits 13-15
- Determine register width from option[0]
- Format as: `[base, offset_reg, extend {#shift}]`

**A64InstructionTable.cpp** (regenerated):
- MEMORY_OFFSET formatter now handles both immediate and register offsets

**Test files**:
- test_ldrb_reg.cpp: Tests LDRB with register offset and UXTW extend

## Technical Details

### ARM64 Register Offset Encoding

```
Bits 30-31: size (00=byte, 01=halfword, 10=word, 11=doubleword)
Bits 16-20: Rm (offset register number)
Bits 13-15: option (extend type)
Bit 12: S (shift enable)
Bits 5-9: Rn (base register)
Bits 0-4: Rt (destination/source register)
```

### Extend Options

| option | Extend | Register | Description |
|--------|--------|----------|-------------|
| 0b000 | UXTB | W | Zero-extend byte |
| 0b001 | UXTH | W | Zero-extend halfword |
| 0b010 | UXTW | W | Zero-extend word |
| 0b011 | LSL | X | Logical shift left |
| 0b100 | SXTB | W | Sign-extend byte |
| 0b101 | SXTH | W | Sign-extend halfword |
| 0b110 | SXTW | W | Sign-extend word |
| 0b111 | SXTX | X | Sign-extend doubleword |

### Shift Amount

When S=1, the offset is shifted by log2(size):
- Byte (size=0): No shift
- Halfword (size=1): Shift by 1
- Word (size=2): Shift by 2
- Doubleword (size=3): Shift by 3

---

**Date**: November 8, 2025
**Issue Fixed**: LDRB register offset showing as immediate (#-12 → w20, uxtw)
**Files Modified**: generate_arm64_disassembler.py
**Tests Added**: test_ldrb_reg.cpp
**Status**: ✅ Complete and tested
