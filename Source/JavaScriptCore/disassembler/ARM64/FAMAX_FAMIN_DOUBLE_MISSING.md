# FAMAX/FAMIN Double-Precision Missing from Instruction Table

## Issue

The instruction table is **missing FAMAX and FAMIN double-precision variants** (.2D arrangement with Q=1).

## Root Cause

The generator incorrectly handles the size field encoding for FAMAX/FAMIN single/double-precision.

### XML Encoding (famax_advsimd.xml)

```xml
<box hibit="23" width="2" name="size" usename="1" settings="1" psbits="xx">
  <c>1</c>
  <c>x</c>
</box>
```

This means:
- **bit[23] = 1** (fixed)
- **bit[22] = x** (variable)
- Valid size values: **10** (single) or **11** (double)

### Current Instruction Table Entry

```cpp
{ "famax", 0xbfe0fc00U, 0x0ea0dc00U, 878, Mnemonic::ARM64_FAMAX, 3, 0 }, // FAMAX_asimdsame_only
```

**Problem**:
- Mask: `0xbfe0fc00` includes **both bits[23:22]** as fixed
- Pattern: `0x0ea0dc00` has `size=10` (bits[23:22]=10)
- This **excludes size=11** (double-precision)!

### Correct Entry Should Be

Either:
1. **Single entry with variable bit[22]**:
   ```cpp
   Mask: 0xbfc0fc00  (bit[22] = 0, variable)
   Pattern: 0x0ea0dc00  (size=10, but bit[22] ignored)
   ```

2. **Two separate entries**:
   ```cpp
   // Single-precision
   { "famax", 0xbfe0fc00U, 0x0ea0dc00U, 878, Mnemonic::ARM64_FAMAX, 3, 0 },

   // Double-precision
   { "famax", 0xbfe0fc00U, 0x0ee0dc00U, 878, Mnemonic::ARM64_FAMAX, 3, 0 },
   ```

## Impact

### Missing Instructions

1. **FAMAX double-precision (U=0)**:
   - Q=0, size=11: UNDEFINED
   - Q=1, size=11: `famax v0.2d, v1.2d, v2.2d` ✗ **NOT IN TABLE**

2. **FAMIN double-precision (U=1)**:
   - Q=0, size=11: UNDEFINED
   - Q=1, size=11: `famin v0.2d, v1.2d, v2.2d` ✗ **NOT IN TABLE**

### Working Instructions

✓ FAMAX half-precision (.4H/.8H) - separate entry for opcode bits[15:10]=000111
✓ FAMAX single-precision (.2S/.4S) - current entry matches size=10
✓ FAMIN half-precision (.4H/.8H) - separate entry for opcode bits[15:10]=000111
✓ FAMIN single-precision (.2S/.4S) - current entry matches size=10

## Workaround for Tests

Test files should **skip** FAMAX/FAMIN double-precision tests until the instruction table is fixed:

```cpp
// FAMAX double - NOT IN TABLE (skip)
// {"FAMAX double", 0x0ee1dc00, 0x4ee1dc00, "skip", "2d"},

// FAMIN double - NOT IN TABLE (skip)
// {"FAMIN double", 0x2ee1dc00, 0x6ee1dc00, "skip", "2d"},
```

## Solution

### Option 1: Fix Generator (Recommended)

Update `generate_arm64_disassembler.py` to handle `psbits="xx"` with partial fixed bits correctly:
- When `settings="1"` and `psbits="xx"` have pattern like "1x"
- Create mask that fixes bit[23]=1 but leaves bit[22] variable
- Mask should be `0xbfc0fc00`, not `0xbfe0fc00`

### Option 2: Manual Patch

Add double-precision entries manually to A64InstructionTable.cpp:

```cpp
// After line 2835 (FAMAX single/double entry)
{ "famax", 0xbfe0fc00U, 0x0ee0dc00U, 878, Mnemonic::ARM64_FAMAX, 3, 0 }, // FAMAX_asimdsame_only (double)
{ "famin", 0xbfe0fc00U, 0x2ee0dc00U, 881, Mnemonic::ARM64_FAMIN, 3, 0 }, // FAMIN_asimdsame_only (double)
```

Note: The arrangement inference logic already handles size=11 correctly, so no code changes needed there!

## Verification

After fix, these opcodes should disassemble correctly:

```
0x4ee1dc00 → famax v0.2d, v0.2d, v1.2d  (currently NOT FOUND)
0x6ee1dc00 → famin v0.2d, v0.2d, v1.2d  (currently NOT FOUND)
```
