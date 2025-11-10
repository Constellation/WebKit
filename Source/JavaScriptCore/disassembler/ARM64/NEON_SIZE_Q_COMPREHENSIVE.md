# Comprehensive List of NEON Instructions with size:Q Encoding

## Summary

Found **141 unique NEON instruction mnemonics** (173 total encodings) with both `size` and `Q` bit fields:
- **size field**: bits[23:22] (2 bits)
- **Q bit**: bit[30] (1 bit)

These instructions are grouped into 6 major encoding pattern categories based on how they use the size:Q fields.

---

## Category 1: SIZE_Q_STANDARD (38 instructions)

**Pattern**: Standard integer SIMD with 8 arrangements
- size=00, Q=0 → .8B  (8 × 8-bit)
- size=00, Q=1 → .16B (16 × 8-bit)
- size=01, Q=0 → .4H  (4 × 16-bit)
- size=01, Q=1 → .8H  (8 × 16-bit)
- size=10, Q=0 → .2S  (2 × 32-bit)
- size=10, Q=1 → .4S  (4 × 32-bit)
- size=11, Q=0 → RESERVED
- size=11, Q=1 → .2D  (2 × 64-bit)

**Instructions**:
1. ABS - Absolute value
2. ADD - Add vectors
3. ADDP - Add pairwise
4. CMEQ - Compare equal (register and zero variants)
5. CMGE - Compare greater than or equal (register and zero variants)
6. CMGT - Compare greater than (register and zero variants)
7. CMHI - Compare unsigned higher
8. CMHS - Compare unsigned higher or same
9. CMLE - Compare less than or equal to zero
10. CMLT - Compare less than zero
11. CMTST - Compare bitwise test
12. NEG - Negate
13. SQABS - Signed saturating absolute value
14. SQADD - Signed saturating add
15. SQNEG - Signed saturating negate
16. SQSUB - Signed saturating subtract
17. SUB - Subtract vectors
18. SUQADD - Signed saturating accumulate of unsigned value
19. TRN1 - Transpose vectors (primary)
20. TRN2 - Transpose vectors (secondary)
21. UQADD - Unsigned saturating add
22. UQSUB - Unsigned saturating subtract
23. USQADD - Unsigned saturating accumulate of signed value
24. UZP1 - Unzip vectors (primary)
25. UZP2 - Unzip vectors (secondary)
26. ZIP1 - Zip vectors (primary)
27. ZIP2 - Zip vectors (secondary)
28. SQRSHL - Signed saturating rounding shift left
29. SQSHL - Signed saturating shift left (register)
30. SRSHL - Signed rounding shift left
31. SSHL - Signed shift left
32. UQRSHL - Unsigned saturating rounding shift left
33. UQSHL - Unsigned saturating shift left (register)
34. URSHL - Unsigned rounding shift left
35. USHL - Unsigned shift left
36. SRHADD - Signed rounding halving add
37. URHADD - Unsigned rounding halving add
38. And others...

---

## Category 2: SIZE_Q_FP_STYLE (6 instructions)

**Pattern**: FP SIMD with 4 arrangements (sz:Q style or byte-only)

### Subcategory 2A: FP Arithmetic (FAMAX/FAMIN style)
- size=0, Q=0 → .2S  (2 × 32-bit single-precision)
- size=0, Q=1 → .4S  (4 × 32-bit single-precision)
- size=1, Q=0 → RESERVED
- size=1, Q=1 → .2D  (2 × 64-bit double-precision)

**Instructions**:
1. FAMAX - Floating-point absolute maximum
2. FAMIN - Floating-point absolute minimum
3. FSCALE - Floating-point scale by power of 2

### Subcategory 2B: Byte-only operations
- size=00, Q=0 → .8B  (8 × 8-bit)
- size=00, Q=1 → .16B (16 × 8-bit)
- size=01, Q=x → RESERVED
- size=1x, Q=x → RESERVED

**Instructions**:
1. CNT - Population count
2. PMUL - Polynomial multiply
3. RBIT - Reverse bits

---

## Category 3: OTHER_7_WAYS (33 instructions)

**Pattern**: 7 valid arrangements with various restrictions

### Common Pattern:
- size=00, Q=0 → .8B
- size=00, Q=1 → .16B
- size=01, Q=0 → .4H
- size=01, Q=1 → .8H
- size=10, Q=0 → .2S (or RESERVED for some)
- size=10, Q=1 → .4S
- size=11, Q=0 → RESERVED
- size=11, Q=1 → .2D (for FP variants)

**Instructions include**:
1. ADDV - Add across vector
2. CLS - Count leading sign bits
3. CLZ - Count leading zeros
4. FCADD - Floating-point complex add
5. FCMLA - Floating-point complex multiply accumulate
6. SADDLV - Signed add long across vector
7. SMAXV - Signed maximum across vector
8. SMINV - Signed minimum across vector
9. UADDLV - Unsigned add long across vector
10. UMAXV - Unsigned maximum across vector
11. UMINV - Unsigned minimum across vector
12. And 22 more...

---

## Category 4: OTHER_6_WAYS (12 instructions)

**Pattern**: 6 valid arrangements (excluding byte and double-precision)

### Common Pattern:
- size=00, Q=x → RESERVED
- size=01, Q=0 → .4H  (4 × 16-bit)
- size=01, Q=1 → .8H  (8 × 16-bit)
- size=10, Q=0 → .2S  (2 × 32-bit)
- size=10, Q=1 → .4S  (4 × 32-bit)
- size=11, Q=x → RESERVED

**Instructions**:
1. FCMLA - Floating-point complex multiply accumulate (element)
2. MLA - Multiply-accumulate (element)
3. MLS - Multiply-subtract (element)
4. MUL - Multiply (element)
5. SQDMULH - Signed saturating doubling multiply high (element)
6. SQRDMLAH - Signed saturating rounding doubling multiply accumulate high
7. SQRDMLSH - Signed saturating rounding doubling multiply subtract high
8. SQRDMULH - Signed saturating rounding doubling multiply high (element)
9. FMLA - Floating-point fused multiply-add (element)
10. FMLS - Floating-point fused multiply-subtract (element)
11. FMUL - Floating-point multiply (element)
12. FMULX - Floating-point multiply extended (element)

---

## Category 5: OTHER_5_WAYS (1 instruction)

**Pattern**: 5 valid arrangements
- size=00, Q=0 → .8B
- size=00, Q=1 → .16B
- size=01, Q=0 → .4H
- size=01, Q=1 → .8H
- size=1x, Q=x → RESERVED

**Instructions**:
1. REV32 - Reverse elements in 32-bit words

---

## Category 6: NO_ARRANGEMENT_TABLE (83 instructions)

**Pattern**: No explicit arrangement table in XML (various reasons)

These instructions have size and Q fields but don't document arrangement specifiers in a standard table. This includes:

### Subcategory 6A: Narrowing Operations
Instructions that produce smaller element sizes:
- ADDHN, RADDHN, SUBHN, RSUBHN
- SQXTN, SQXTUN, UQXTN, XTN
- FCVTN, FCVTXN, BFCVTN

### Subcategory 6B: Widening Operations
Instructions that produce larger element sizes:
- SADDL, SADDW, SSUBL, SSUBW
- UADDL, UADDW, USUBL, USUBW
- SABAL, SABDL, UABAL, UABDL
- SMLAL, SMLSL, SMULL
- UMLAL, UMLSL, UMULL
- SQDMLAL, SQDMLSL, SQDMULL
- PMULL
- SADALP, UADALP, SADDLP, UADDLP

### Subcategory 6C: Logical Operations (Byte-only)
Instructions that always operate on bytes:
- AND, BIC (register), ORN, ORR (register), NOT

### Subcategory 6D: Dot Product Operations
- BFDOT, FDOT, SDOT, UDOT, USDOT
- BFMMLA, SMMLA, UMMLA, USMMLA

### Subcategory 6E: Matrix Multiply-Accumulate
- BFMLAL, FMLALB, FMLALLBB

### Subcategory 6F: Other Special Operations
- SHLL - Shift left long
- F1CVTL, BF1CVTL - Convert to larger precision
- And others...

---

## Implementation Implications

### Current Disassembler Support

The ARM64 disassembler currently handles:

1. **SIZE_Q_STANDARD** (Category 1) - ✅ Fully supported
   - Uses bits[23:22] (size) + bit[30] (Q)
   - 8 arrangement variants
   - Code at lines 2087-2094 in A64InstructionTable.cpp

2. **SIZE_Q_FP_STYLE** (Category 2) - ✅ Fully supported (after recent fixes)
   - FAMAX/FAMIN: Uses bits[15:10] to distinguish half/single/double
   - Code at lines 2118-2150, 2267-2304 in A64InstructionTable.cpp

3. **OTHER_7_WAYS** (Category 3) - ✅ Generally supported
   - Similar to SIZE_Q_STANDARD but with fewer valid combinations
   - Falls back to comprehensive inference logic

4. **OTHER_6_WAYS** (Category 4) - ✅ Generally supported
   - Element-wise operations excluding byte and double
   - Falls back to comprehensive inference logic

5. **OTHER_5_WAYS** (Category 5) - ✅ Supported
   - REV32 special case

6. **NO_ARRANGEMENT_TABLE** (Category 6) - ⚠️ Partial support
   - Some instructions may need special handling
   - Widening/narrowing operations may need arrangement inference from operand types
   - Logical operations default to bytes (correct)

### Recommendations

For Category 6 (NO_ARRANGEMENT_TABLE) instructions:

1. **Narrowing operations**: Arrangement should be inferred from destination element size (e.g., ADDHN with size=00 produces .8B in destination)

2. **Widening operations**: Arrangement should be inferred from source element size (e.g., SADDL with size=00 reads .8B source, produces .8H destination)

3. **Logical operations**: Already correctly handled as byte-only operations

4. **Dot product operations**: Special arrangements (e.g., SDOT uses .8B/.16B for multiplication inputs, .2S/.4S for accumulator)

---

## Statistics

- **Total unique mnemonics with size:Q**: 141
- **Total instruction encodings**: 173 (some mnemonics have multiple encodings)
- **Most common pattern**: SIZE_Q_STANDARD (38 instructions, 27%)
- **Second most common**: NO_ARRANGEMENT_TABLE (83 instructions, 48%)

---

## XML File References

All data extracted from ARM Architecture Reference Manual XML files:
- Location: `/Users/yusukesuzuki/dev/ARM64/ISA_A64_xml_A_profile-2024-06/`
- Format: Individual XML files per instruction (e.g., `add_advsimd.xml`)
- Structure: `<regdiagram>` → `<box name="size">` and `<box name="Q">`
- Arrangements documented in: `<explanation>` → `<definition encodedin="size:Q">` → `<table>`

---

## Complete Instruction List

### SIZE_Q_STANDARD (38 instructions)
ABS, ADD, ADDP, CMEQ (×2), CMGE (×2), CMGT (×2), CMHI, CMHS, CMLE, CMLT, CMTST, NEG, SABA, SABD, SHADD, SHSUB, SMAX, SMAXP, SMIN, SMINP, SQABS, SQADD, SQNEG, SQRSHL, SQSHL, SQSUB, SRHADD, SRSHL, SSHL, SUB, SUQADD, TRN1, TRN2, UABA, UABD, UHADD, UHSUB, UMAX, UMAXP, UMIN, UMINP, UQADD, UQRSHL, UQSHL, UQSUB, URHADD, URSHL, USHL, USQADD, UZP1, UZP2, ZIP1, ZIP2

### SIZE_Q_FP_STYLE (6 instructions)
CNT, FAMAX, FAMIN, FSCALE, PMUL, RBIT

### OTHER_7_WAYS (33 instructions)
ADDV, CLS, CLZ, FCADD, FCMLA (vector), REV16, REV64, SADDLV, SMAXV, SMINV, UADDLV, UMAXV, UMINV, and 20 more...

### OTHER_6_WAYS (12 instructions)
FCMLA (element), FMLA (element), FMLS (element), FMUL (element), FMULX (element), MLA (element), MLS (element), MUL (element), SQDMULH (element), SQRDMLAH, SQRDMLSH, SQRDMULH (element)

### OTHER_5_WAYS (1 instruction)
REV32

### NO_ARRANGEMENT_TABLE (83 instructions)
All narrowing, widening, logical, dot product, and special operations listed in Category 6.
