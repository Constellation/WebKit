# LDRB Zero Shift Display Fix

## Problem
LDRB (and other byte load/store instructions) were displaying "lsl #0" when the S bit was set but the calculated shift amount was 0:

```
ldrb     w0, [x22, x0, lsl #0]  ← Incorrect
```

Should be:
```
ldrb     w0, [x22, x0]  ← Correct
```

## Root Cause

The memory register offset handler was checking only the S bit (bit 12) to decide whether to display shift, but not the calculated shift amount.

For LDRB:
- Bits [30:31] = 00 (byte access size)
- When S=1, shift_amount = extractBits(opcode, 30, 2) = 0
- Result: "lsl #0" was displayed

## Solution

Modified the logic to calculate shift amount FIRST, then use it in the decision:

```cpp
// Calculate actual shift amount first
uint32_t shift_amount = 0;
if (shift) {
    shift_amount = extractBits(opcode, 30, 2);
}

// Check if this is LSL with X register and no actual shift
// No shift means: S=0 OR calculated shift_amount is 0
bool is_x_lsl_no_shift = \!use_w_reg && (option == 3 || option == 7) && (shift_amount == 0);

// Only output extend/shift if NOT (X register LSL without shift)
if (\!is_x_lsl_no_shift) {
    offset += snprintf(buffer + offset, bufferSize - offset, ", %s", extend_names[option & 0x7]);

    // Add shift amount if non-zero
    if (shift_amount > 0) {
        offset += snprintf(buffer + offset, bufferSize - offset, " #%u", shift_amount);
    }
}
```

## Test Cases

All 5 test cases pass:

1. **LDRB S=0**: `ldrb w0, [x22, x0]` ✓
2. **LDRB S=1, shift=0**: `ldrb w0, [x22, x0]` ✓ (was showing "lsl #0")
3. **LDRH S=1, shift=1**: `ldrh w0, [x22, x0, lsl #1]` ✓
4. **LDR W S=1, shift=2**: `ldr w0, [x22, x0, lsl #2]` ✓
5. **LDR X S=1, shift=3**: `ldr x0, [x22, x0, lsl #3]` ✓

## Opcodes

- 0x38606ac0: LDRB S=0 (no shift bit)
- 0x38607ac0: LDRB S=1 (shift bit set but amount=0) ← Key test case
- 0x78607ac0: LDRH S=1 (shift amount=1)
- 0xb8607ac0: LDR W S=1 (shift amount=2)
- 0xf8607ac0: LDR X S=1 (shift amount=3)

## Impact

This fix ensures that:
1. Zero shifts are never displayed for X register LSL addressing
2. Works for both S=0 (no shift bit) and S=1 with zero amount (byte access)
3. Non-zero shifts are still correctly displayed
4. All 112 tests pass (previous 107 + new 5)

## Files Modified

- `generate_arm64_disassembler.py` (lines 2390-2421)
- `MEMORY_ADDRESSING_SHIFT_FIX.md` (updated documentation)

## Test File Created

- `test_ldrb_shift.cpp` - Comprehensive test for LDRB/LDRH zero shift edge cases
