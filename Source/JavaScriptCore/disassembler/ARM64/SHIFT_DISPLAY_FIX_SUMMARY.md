# ADD/SUB/CMP Immediate Shift Display Fix

## Issue
CMP and other ADD/SUB immediate instructions were always showing "lsl" even when there was no actual shift (sh bit = 0).

**Example:**
```
Before: cmp x0, #0, lsl      (incorrect - trailing "lsl")
After:  cmp x0, #0           (correct - no shift shown)

Before: cmp x0, #0, lsl      (incorrect - missing #12)
After:  cmp x0, #0, lsl #12  (correct - shows shift amount)
```

## Root Cause
ADD/SUB/ADDS/SUBS immediate instructions use the sh bit (bit 22) to encode shift:
- **sh = 0**: No shift (LSL #0) → Should not be displayed
- **sh = 1**: LSL #12 → Should be displayed as ", lsl #12"

The parser created a SHIFT_TYPE operand without field bindings (field1_width=0, field2_width=0), which meant:
1. The formatter couldn't read the sh bit to determine if shift should be shown
2. It always output "lsl" (default shift name) without checking if shift was actually present
3. It couldn't output the shift amount since field2_width was 0

## Solution
Implemented a two-part fix:

### Part 1: Pre-check in Operand Loop (Lines 1620-1628)
```cpp
// Pre-check: Skip SHIFT_TYPE operands with no bindings when sh=0
if (op.type == 51 && op.field1_width == 0 && op.field2_width == 0) {
    uint32_t sh = extractBits(opcode, 22, 1);
    if (sh == 0) {
        // No shift, skip this operand entirely
        continue;
    }
}
```

**Purpose**: When sh=0 (no shift), skip the entire SHIFT_TYPE operand, including its separator. This prevents "cmp x0, #0, " with trailing comma.

### Part 2: Special Handling in SHIFT_TYPE Case (Lines 2302-2323)
```cpp
case 51: // SHIFT_TYPE
    if (op.field1_width == 0 && op.field2_width == 0) {
        // No field bindings - check sh bit directly
        uint32_t sh = extractBits(opcode, 22, 1);
        if (sh) {
            // sh=1 means LSL #12
            offset += snprintf(buffer + offset, bufferSize - offset, "lsl #12");
        }
    } else {
        // Normal shift operand with field bindings
        offset += snprintf(buffer + offset, bufferSize - offset, "%s",
                         g_shiftNames[field1_val & 0x3]);
        if (field2_val && op.field2_width > 0) {
            offset += snprintf(buffer + offset, bufferSize - offset,
                             " #%u", field2_val);
        }
    }
    break;
```

**Purpose**: When sh=1 (shift present), output "lsl #12" correctly.

## Key Design Decisions

### Why Check sh Bit Before Separator?
The separator logic adds ", " before each operand (except the first). If we skip an operand with `continue` in the pre-check, the separator is never added, resulting in clean output without trailing commas.

### Why Check Field Widths?
Checking `field1_width == 0 && field2_width == 0` distinguishes between:
- **ADD/SUB immediate**: No field bindings, implicit LSL, sh bit determines amount
- **Shifted register operations**: Normal field bindings, explicit shift type and amount

### Why Hardcode "lsl #12"?
For ADD/SUB immediate instructions:
- Shift type is always LSL (implicit in encoding)
- Shift amount is always 0 or 12 (determined by sh bit)
- No other combinations are possible

## Test Results

### CMP Instructions (9/9 pass ✓)
- CMP with no shift: 3/3 ✓
- CMP with lsl #12: 2/2 ✓
- CMP register (no shift): 2/2 ✓
- CMP register with extend: 2/2 ✓

### ADD/SUB/ADDS/SUBS Instructions (16/16 pass ✓)
- ADD: 4/4 ✓ (with/without shift)
- SUB: 4/4 ✓ (with/without shift)
- ADDS: 4/4 ✓ (with/without shift)
- SUBS: 4/4 ✓ (with/without shift)

### Regression Tests (All Pass ✓)
- Q-only arrangements: 16/16 ✓
- TBL: 5/5 ✓
- FMUL: 6/6 ✓
- LD1R: 8/8 ✓

## Files Modified
1. `generate_arm64_disassembler.py` - Lines 1620-1628, 2302-2323
2. `test_cmp.cpp` - Created: CMP shift test suite
3. `test_addsub_shift.cpp` - Created: ADD/SUB shift test suite

## Benefits
- **Correct output**: No trailing "lsl" when shift is not present
- **Complete output**: Shows "lsl #12" with proper amount when shift is present
- **No trailing commas**: Clean separator handling
- **Backwards compatible**: Normal shifted register operations unaffected
- **Comprehensive**: Covers ADD, SUB, ADDS, SUBS, CMP instructions

## Example Outputs

### Before Fix
```
cmp      x0, #0, lsl           ✗
cmp      x0, #1, lsl           ✗
add      x0, x1, #0, lsl       ✗
sub      x0, x1, #1, lsl #12, lsl  ✗
```

### After Fix
```
cmp      x0, #0                ✓
cmp      x0, #1                ✓
add      x0, x1, #0            ✓
sub      x0, x1, #1, lsl #12   ✓
```
