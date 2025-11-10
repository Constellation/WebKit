# SVE/SME Test Results

## Test Coverage

Created comprehensive test suite for SVE (Scalable Vector Extension) and SME (Scalable Matrix Extension) instructions using objdump as reference.

### Test Categories
1. **SVE Basic Arithmetic** (unpredicated)
2. **SVE Predicated Operations**  
3. **SVE Floating Point** (FMUL)
4. **SVE Load/Store** (LD1W, LD1D, ST1W, ST1D)
5. **SVE Compare** (FCMGE)
6. **SVE Logical** (AND, ORR, EOR)
7. **FAMAX SIMD** (various element sizes)
8. **FAMAX SVE** (predicated)

### Test Opcodes (28 total)
All opcodes verified against objdump for ARM v9 + SVE.

## Current Status

### ✅ What Works
1. **Instruction Recognition**: All 28 SVE/SME instructions successfully found in table
2. **Basic Decoding**: Mnemonics correctly identified
3. **Hash Table Lookup**: Fast instruction finding works for SVE/SME
4. **No Crashes**: All tests complete successfully

### ⚠️ Formatting Issues Found

#### 1. SVE Unpredicated Arithmetic - Missing Element Sizes
**Expected**: `add z0.b, z1.b, z2.b`
**Got**: `add z0, z1, z2`

Affects:
- ADD (4 variants: .b, .h, .s, .d)
- FMUL (3 variants: .h, .s, .d)
- AND, OR, EOR logical operations

**Root Cause**: SVE register operands need element size suffix based on instruction encoding

#### 2. SVE Predicated Operations - Wrong Format
**Expected**: `add z0.b, p0/m, z0.b, z1.b`
**Got**: `add d0, p0, d0, z1`

Affects:
- All predicated ADD variants
- FAMAX SVE variants

**Root Cause**:
- Register type wrong (showing 'd0' instead of 'z0')
- Missing predicate modifier '/m'
- Missing element size specifiers

#### 3. SVE Load/Store - Missing Register List
**Expected**: `ld1w { z0.s }, p0/z, [x0]`
**Got**: `ld1w p0, [x0]`

Affects:
- LD1W, LD1D
- ST1W, ST1D

**Root Cause**: SVE load/store needs register in braces with predicate modifier '/z'

#### 4. FAMAX SIMD - Wrong Operands
**Expected**: `famax v0.2s, v1.2s, v2.2s`
**Got**: `famax v0.8b, v0.8b, v0.8b`

Affects:
- FAMAX 2S/4S variants
- FAMAX 4H/8H variants

**Root Cause**: Operand field extraction getting wrong registers and element sizes

## Detailed Comparison

| Opcode     | Expected (objdump)                | Our Output                    | Status |
|------------|-----------------------------------|-------------------------------|--------|
| 0x04220020 | add z0.b, z1.b, z2.b             | add z0, z1, z2                | ⚠️     |
| 0x04000020 | add z0.b, p0/m, z0.b, z1.b       | add d0, p0, d0, z1            | ❌     |
| 0x65420820 | fmul z0.h, z1.h, z2.h            | fmul z0, z1, z2               | ⚠️     |
| 0xa540a000 | ld1w { z0.s }, p0/z, [x0]        | ld1w p0, [x0]                 | ❌     |
| 0x0ea0dc00 | famax v0.2s, v1.2s, v2.2s        | famax v0.8b, v0.8b, v0.8b     | ❌     |
| 0x650e8000 | famax z0.h, p0/m, z0.h, z0.h     | famax d0, p0, d0, z0          | ❌     |

Full comparison: 28 instructions tested, 0 crashes, formatting issues in ~60% of cases.

## What Needs Enhancement

### High Priority
1. **SVE Element Size Formatting**
   - Extract size from instruction encoding
   - Append to Z register format (z0.b, z0.h, z0.s, z0.d)

2. **SVE Predicate Modifiers**
   - Add '/m' (merging) or '/z' (zeroing) after predicate registers
   - Format: `p0/m` instead of just `p0`

3. **SVE Register Type Detection**
   - Distinguish Z registers from D registers
   - Use correct prefix based on operand type

### Medium Priority  
4. **SVE Load/Store Formatting**
   - Format register lists in braces: `{ z0.s }`
   - Include predicate with modifier: `p0/z`

5. **FAMAX Operand Extraction**
   - Fix field extraction for SIMD variants
   - Ensure correct register numbers (v0, v1, v2 not v0, v0, v0)

### Implementation Guidance

#### SVE Element Size
```cpp
// In formatInstruction for REG_SVE_Z:
// Extract size from instruction bits 22-23 for unpredicated
// or from instruction context for predicated
char sizeChar = '.?';  // b, h, s, d based on encoding
offset += snprintf(buffer + offset, bufferSize - offset, "z%u%c", regNum, sizeChar);
```

#### Predicate Modifier
```cpp
// For SVE predicated instructions:
// Check governing predicate flag in instruction
const char* modifier = "/m";  // or "/z" for zeroing
offset += snprintf(buffer + offset, bufferSize - offset, "p%u%s", predNum, modifier);
```

## Test Files Created

1. **sve_test.s** - Assembly source for SVE instructions
2. **sve_test.o** - Assembled object file
3. **sve_test.dis** - objdump reference output  
4. **test_sve_sme.cpp** - C++ test suite (28 test cases)
5. **sve_comparison.txt** - Detailed comparison table
6. **SVE_SME_TEST_RESULTS.md** - This document

## Usage

```bash
# Compile and run SVE/SME tests
clang++ -std=c++20 -I. -o test_sve_sme test_sve_sme.cpp A64InstructionTable.cpp
./test_sve_sme

# Compare with objdump (for reference)
as -march=armv9-a+sve sve_test.s -o sve_test.o
objdump -d sve_test.o > sve_test.dis
```

## Next Steps

To fully support SVE/SME disassembly:

1. **Enhance XML Parser** (generate_arm64_disassembler.py)
   - Parse SVE-specific operand attributes
   - Extract element size encoding information
   - Identify predicate modifiers

2. **Update Operand Types**
   - Add REG_SVE_Z_SIZED (with .b/.h/.s/.d suffix)
   - Add REG_SVE_P_MODIFIED (with /m or /z)
   - Add special SVE load/store list formatting

3. **Extend Formatter** (formatInstruction)
   - Implement element size extraction for SVE
   - Add predicate modifier logic
   - Handle SVE register list braces

4. **Validate**
   - Re-run test suite
   - Compare all outputs with objdump
   - Add more SVE/SME instruction variants

## Conclusion

✅ **Test infrastructure complete**
- 28 SVE/SME instructions tested
- Reference output from objdump documented
- Clear comparison showing formatting gaps

⚠️ **Formatting needs enhancement**
- Element size suffixes missing
- Predicate modifiers missing  
- Some operand extraction issues

The foundation is solid - instruction recognition works perfectly. Enhancement needed in operand formatting to match ARM standard SVE/SME syntax.
