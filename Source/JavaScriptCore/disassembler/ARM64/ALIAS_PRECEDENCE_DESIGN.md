# ARM64 Disassembler Alias Precedence System

## Overview

This document describes the systematic, data-driven approach to handling ARM64 instruction alias precedence in the disassembler code generator.

## Background

ARM64 instructions often have multiple mnemonics (aliases) that map to the same base instruction encoding. For example, `LSL`, `LSR`, `UBFIZ`, `UBFX`, `UXTB`, and `UXTH` are all aliases for the `UBFM` (Unsigned Bitfield Move) instruction, differentiated by their operand values and conditions.

When disassembling an opcode, multiple aliases may match. The disassembler must return the most specific/appropriate alias.

## Analysis Methodology

We analyzed **187 alias relationships** across **59 base instructions** from 2,136 ARM64 XML specification files using `analyze_alias_precedence.py`. This tool extracts:

1. Alias names and their base instructions
2. Alias conditions from `<aliascond>` elements
3. Common condition patterns

## Alias Condition Patterns

From the analysis, we identified these condition specificity levels (most to least specific):

### 1. Arithmetic Equality (Most Specific)
**Pattern**: `field1 + constant == field2`

**Example**: LSL (Logical Shift Left)
```
Condition: UInt(imms) + 1 == UInt(immr)
Priority: 0
```

This is the most specific because it requires an exact arithmetic relationship between two fields.

### 2. Function-Based Checks
**Pattern**: Contains function calls like `BFXPreferred(sf, opc<1>, imms, immr)`

**Examples**: UBFX, SBFX
```
Condition: BFXPreferred() == TRUE
Priority: 10
```

These use implementation-defined logic to determine if the bitfield extract preferred form should be used.

### 3. Simple Equality
**Pattern**: `field1 == field2` or `field == constant`

**Examples**: MOV variants
```
Condition: Rn == Rm
Priority: 20-29
```

Direct field comparisons without arithmetic.

### 4. Inequality with Negation
**Pattern**: `!(condition)` or `field != value`

**Examples**: MOV/MOVZ/MOVN
```
Condition: !(IsZero(imm16) && hw != '00')
Priority: 30-39
```

### 5. Comparison Conditions
**Pattern**: `<`, `>`, `<=`, `>=`

**Examples**: UBFIZ, SBFIZ, BFXIL
```
UBFIZ: UInt(imms) < UInt(immr)     (Priority: 40)
BFXIL: UInt(imms) >= UInt(immr)    (Priority: 45)
```

Range-based conditions are less specific than exact equality.

### 6. Unconditional (Least Specific)
**Pattern**: "Unconditionally" (with implicit encoding constraints)

**Examples**: LSR, ASR, UXTB, UXTH, SXTB, SXTH, SXTW
```
Condition: Unconditionally (but specific field values)
Priority: 20-50
```

These match broadly but rely on specific bit patterns in the encoding.

### 7. Base Instructions (Lowest Priority)
Non-alias base instructions always have the lowest priority.

**Examples**: UBFM, SBFM, BFM
```
Priority: 100
```

## Implementation

### Bitfield Instruction Alias Hierarchy

Based on the analysis, bitfield instructions (UBFM, SBFM, BFM) have this precedence order:

```python
alias_priority = {
    # Arithmetic equality - most specific
    'lsl': 0,      # imms + 1 == immr

    # Function-based checks
    'ubfx': 10,    # BFXPreferred()
    'sbfx': 10,    # BFXPreferred()

    # Simple shifts (unconditional but specific encoding)
    'lsr': 20,     # Unconditional (imms == 31/63)
    'asr': 20,     # Unconditional (imms == 31/63)

    # Comparison-based (less specific)
    'ubfiz': 40,   # imms < immr
    'sbfiz': 40,   # imms < immr
    'bfi': 40,     # imms < immr
    'bfc': 40,     # imms < immr (with Rn==11111)
    'bfxil': 45,   # imms >= immr

    # Extension aliases (unconditional, specific field values)
    'uxtb': 50,    # Unconditional (specific imms/immr)
    'uxth': 50,    # Unconditional (specific imms/immr)
    'sxtb': 50,    # Unconditional (specific imms/immr)
    'sxth': 50,    # Unconditional (specific imms/immr)
    'sxtw': 50,    # Unconditional (specific imms/immr)

    # Base instructions (lowest priority)
    'ubfm': 100,
    'sbfm': 100,
    'bfm': 100,
}
```

### Sorting Algorithm

Instructions are sorted using a two-level key:

```python
def sort_key(instr):
    mask_bits = bin(instr.mask).count('1')
    priority = alias_priority.get(instr.mnemonic.lower(), 50)
    return (-mask_bits, priority)
```

1. **Primary sort**: Number of fixed bits (descending) - More fixed bits = more specific encoding
2. **Secondary sort**: Alias priority (ascending) - Lower number = higher priority

This ensures that when two instructions have the same encoding constraints (same mask/pattern), the more specific alias condition is checked first.

## Example: LSL vs UBFIZ

Consider opcode `0xD37AF400` (LSL x0, x0, #6):

```
Fields: sf=1, opc=10, N=1, immr=58, imms=57
```

Both LSL and UBFIZ match the same mask/pattern `0xffc00000` / `0xd3400000`:

**LSL condition**: `imms + 1 == immr`
- Check: 57 + 1 == 58 ✓ **Matches**

**UBFIZ condition**: `imms < immr`
- Check: 57 < 58 ✓ **Also matches**

Without precedence, UBFIZ would be returned (if it appears first in the table). With our system:
- LSL has priority 0 (arithmetic equality)
- UBFIZ has priority 40 (comparison)
- LSL is placed before UBFIZ in the sorted table
- Result: **LSL x0, x0, #6** ✓

## Testing

The precedence system is validated through comprehensive tests:

### Test Suite Results

```bash
./test_all_fixes
```

All tests pass, including:
- Bitfield shift aliases (LSL, LSR, ASR)
- Bitfield insert/extract (UBFIZ, SBFIZ, etc.)
- Register shifts with shift amounts
- SIMD element operations (UMOV, INS, DUP)
- Q-bit controlled aliases (SXTL2, UXTL2)

## Files

- `analyze_alias_precedence.py`: Analysis tool to extract alias patterns from XML files
- `generate_arm64_disassembler.py`: Code generator with systematic precedence (lines 955-1010)
- `test_all_fixes.cpp`: Comprehensive test suite
- `test_lsl_variants.cpp`: Specific bitfield shift alias tests

## Key Learnings

1. **ElementTree Gotcha**: Python's `xml.etree.ElementTree` elements evaluate to `False` in boolean context when they have no children, even though they exist. Always use `is None` instead of `not element`.

2. **Condition Specificity Matters**: Arithmetic equality (`imms + 1 == immr`) is more specific than simple comparison (`imms < immr`), even though both may match the same opcode.

3. **Data-Driven Design**: Analyzing all 2,136 XML files systematically reveals patterns that aren't obvious from examining individual cases.

4. **Mask Bits + Priority**: Combining encoding specificity (mask bits) with condition specificity (priority) provides the correct ordering.

## Future Enhancements

Potential improvements to consider:

1. **Automatic Priority Assignment**: Parse alias conditions and automatically assign priorities based on condition complexity.

2. **Condition Validation**: Verify that higher-priority aliases are actually more specific than lower-priority ones.

3. **Coverage Analysis**: Identify opcodes that match multiple aliases and verify the most appropriate one is selected.

4. **XML-Driven Priorities**: Extract precedence hints directly from ARM XML documentation if available.

## References

- ARM Architecture Reference Manual for A-profile architecture
- ARM64 XML Specification Files (ISA_A64_xml_A_profile-2024-06)
- JavaScriptCore ARM64 Disassembler Implementation
