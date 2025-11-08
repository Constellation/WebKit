# SIMD Operand Pattern Support Implementation

**Date**: November 8, 2025
**Status**: ✅ Completed
**Impact**: Fixed 71 instructions (52% reduction in zero-operand instructions)

## Summary

Implemented comprehensive support for SIMD/FP scalar operand patterns that use single-letter register numbers with size specifiers (V_option, T_option). This fix addresses the core issue where instructions like FCVTAS showed no operands because the parser couldn't handle patterns like `<V><d>` where `V_option` determines register size and `d` is the register number.

## Problem Statement

### Before
- **136 zero-operand instructions** (3.4% of 4,013 total)
- Instructions like `fcvtas s0, s1` showed as `fcvtas` with no operands
- Many FP conversion, compare, and saturating arithmetic instructions missing operands

### Root Causes
1. **No V_option/T_option tracking**: Parser didn't track size specifiers
2. **Single-letter links not recognized**: Links like `d`, `n`, `m` were ignored
3. **Case sensitivity**: Parser checked lowercase, XML had mixed case (though this was already handled)
4. **Suffix numbers**: Links like `n__3` needed suffix stripping

### After
- **65 zero-operand instructions** (1.6% of total) - **71 instructions fixed!**
- FCVTAS and similar instructions now show correct operands
- Remaining zero-operand instructions are mostly system/hint instructions

## Implementation Details

### 1. V_option and T_option Tracking

**File**: `generate_arm64_disassembler.py`
**Location**: `_parse_asmtemplate()` function, lines 252-282

Added tracking for size specifiers similar to existing R_option:

```python
last_r_option = None  # Track R_option (width specifier) for next register
last_v_option = None  # Track V_option (SIMD width specifier) for next register
last_t_option = None  # Track T_option (element size specifier) for next register

# Check if this is V_option (SIMD width specifier)
if link_lower.startswith('v_option'):
    # Store hover and link to determine SIMD register size
    last_v_option = (hover, link)
    i += 1
    continue

# Check if this is T_option (element size specifier)
if link_lower.startswith('t_option'):
    # Store hover and link to determine element size
    last_t_option = (hover, link)
    i += 1
    continue
```

**Why**: V_option/T_option appear before register number operands and specify the register size (B/H/S/D/Q). The parser needs to remember these to interpret the following single-letter register numbers.

### 2. Single-Letter Register Detection

**File**: `generate_arm64_disassembler.py`
**Location**: `_infer_operand()` function, lines 450-478

Detects single-letter links and uses stored option context:

```python
# Check for single-letter register numbers with V_option or T_option context
# These are register numbers (d, n, m, t, a) that depend on a preceding size specifier
if re.match(r'^[dnmta](__\d+)?$', link_lower):
    # This is a single-letter register number
    if v_option_data:
        # V_option specifies the register size (B/H/S/D/Q)
        # Extract the field name from V_option hover (e.g., "sz", "size", "Q")
        v_hover, v_link = v_option_data
        # Get the field name for the size specifier
        v_field_names = re.findall(r'"([A-Za-z0-9_:]+)"', v_hover)
        size_field = v_field_names[0] if v_field_names else 'size'
        # Return a sized SIMD register operand
        # The size_field determines B/H/S/D/Q at runtime
        return Operand('REG_SIMD_SIZED', None, primary_field or 'Rd', size_field, is_optional,
                      f"SIMD register with size from {size_field}")
```

**Why**: XML uses patterns like:
```xml
<V><d>  where V_option = "sz" field, d = "Rd" field
```
The single letter `d` is just a register number. The size comes from the preceding `V_option`.

### 3. Suffix Number Stripping

**File**: `generate_arm64_disassembler.py`
**Location**: `_infer_operand()` function, line 451

```python
# Strip suffix numbers from link patterns (__2, __3, etc.) for better matching
link_base = re.sub(r'__\d+$', '', link_lower)
```

**Why**: XML uses variants like `n`, `n__2`, `n__3` for multiple registers. The suffix is just a disambiguator.

### 4. REG_SIMD_SIZED Operand Type

**File**: `generate_arm64_disassembler.py`

#### Python Type Mapping (line 654)
```python
'REG_SIMD_SIZED': 16,  # SIMD register with size determined by field
```

#### C++ Formatter (lines 1085-1114)
```cpp
case 16: // REG_SIMD_SIZED (size determined by field2)
    // field1 = register number (Rd, Rn, etc.)
    // field2 = size field (sz, size, Q, etc.)
    // Map size field value to register prefix
    {
        char prefix;
        // Common mappings:
        // 1-bit sz: 0=S, 1=D
        // 2-bit size: 00=B, 01=H, 10=S, 11=D
        // 1-bit Q: 0=D, 1=Q
        if (op.field2_width == 1) {
            // 1-bit field: sz or Q
            if (op.field2_start == 30) {
                // Q bit (bit 30) determines scalar size
                prefix = field2_val ? 'q' : 'd';
            } else {
                // sz bit determines FP size
                prefix = field2_val ? 'd' : 's';
            }
        } else if (op.field2_width == 2) {
            // 2-bit size field
            const char size_map[] = {'b', 'h', 's', 'd'};
            prefix = size_map[field2_val & 3];
        } else {
            // Default to 's' if unknown
            prefix = 's';
        }
        offset += snprintf(buffer + offset, bufferSize - offset, "%c%u", prefix, field1_val);
    }
    break;
```

**How it works**:
1. Field1 contains register number (Rd, Rn, etc.)
2. Field2 contains size field (sz, size, Q, etc.)
3. Formatter extracts both values from opcode
4. Maps size field value to register prefix (b/h/s/d/q)
5. Formats as `<prefix><reg_num>` (e.g., `s1`, `d3`)

## Size Field Mappings

### 1-bit sz field
- `sz=0` → Single precision (s)
- `sz=1` → Double precision (d)

### 2-bit size field
- `size=00` → Byte (b)
- `size=01` → Halfword (h)
- `size=10` → Single/Word (s)
- `size=11` → Double (d)

### 1-bit Q field (bit 30)
- `Q=0` → Double (d)
- `Q=1` → Quad (q)

## Testing

### Test Case: FCVTAS
```cpp
// FCVTAS (scalar) - Convert scalar single to signed integer
// fcvtas w0, s1
uint32_t opcode1 = 0x1E240020;
// Result: fcvtas   w0, s1 ✓

// FCVTAS (scalar) - Convert scalar double to signed integer
// fcvtas x2, d3
uint32_t opcode2 = 0x9E640062;
// Result: fcvtas   x2, d3 ✓
```

### Existing Tests Status
All previous tests continue to pass:
- ✅ ADD with extend
- ✅ Conditional branches
- ✅ CASAL
- ✅ TST with logical immediate
- ✅ TBNZ with bit position
- ✅ LDRB with register offset
- ✅ MOV/MOVK immediate formatting

## Impact Analysis

### Instructions Fixed by Category

Based on the 71 instructions fixed:
- **FP Conversion** (fcvtas, fcvtns, etc.): ~11 instructions
- **FP Compare** (fcmgt, fcmge, fcmle, etc.): ~12 instructions
- **SIMD Saturating** (sqd*, sqr*, sqx*): ~8 instructions
- **Other FP/SIMD scalar operations**: ~40 instructions

### Remaining Zero-Operand Instructions (65)

By category:
- **PAC/AUT** (Pointer Authentication): 21 instructions
- **Hints** (yield, wfe, sev, etc.): ~10 instructions
- **GCS** (Guarded Control Stack): 5 instructions
- **System** (eret, ret variants): ~5 instructions
- **Other**: 24 instructions

Most remaining instructions either:
1. Have no operands (hint instructions)
2. Use specialized operand types not yet implemented
3. Are rarely used in JavaScript engines

## Code Changes Summary

### Modified Functions

1. **`_parse_asmtemplate()`** (lines 250-328)
   - Added `last_v_option` and `last_t_option` tracking
   - Pass option data to `_infer_operand()`
   - Reset option context after use

2. **`_infer_operand()`** (lines 421-532)
   - Updated signature to accept `v_option_data` and `t_option_data`
   - Added suffix stripping (line 451)
   - Added single-letter register detection (lines 455-478)
   - Uses stored option context to create REG_SIMD_SIZED operands

3. **`OP_TYPES` dictionary** (line 654)
   - Added `'REG_SIMD_SIZED': 16`

4. **Formatter switch** (lines 1085-1114)
   - Added case 16 for REG_SIMD_SIZED
   - Implements runtime size mapping based on field values

## Performance Considerations

- **Build time**: No significant change (still ~10 seconds for 2,136 XML files)
- **Runtime**: Minimal overhead (one additional switch case, simple arithmetic)
- **Memory**: No change (operand table size unchanged)

## Future Enhancements

### Phase 2: Vector Register Support (not implemented)

Would handle patterns like:
```xml
<V><d>.<T>  where V_option = vector register, T = element size
```
Example: `add v0.2s, v1.2s, v2.2s`

This requires:
1. Tracking element size specifiers
2. Formatting as `v0.2s` instead of `s0`
3. Handling vector arrangements (.2s, .4h, .8b, etc.)

**Priority**: Medium (needed for SIMD vector operations)

### Phase 3: SVE/SME Support (not implemented)

Would handle `sa_*` prefix patterns for Scalable Vector Extension.

**Priority**: Low (rarely used in JavaScript engines)

## Documentation

### Related Files
- `OPERAND_COVERAGE_ANALYSIS.md` - Original problem analysis
- `debug_fcvtas.py` - Tool for debugging FCVTAS parsing
- `analyze_operands.py` - Extracts all link patterns from XML
- `check_parser_coverage.py` - Shows handled patterns
- `analyze_table.py` - Identifies zero-operand instructions
- `test_fcvtas.cpp` - Test cases for FCVTAS

### Commit Message Template
```
ARM64 Disassembler: Add support for SIMD/FP scalar operands with size specifiers

Implement V_option/T_option tracking and single-letter register number
detection to handle FP/SIMD scalar instructions. Fixes 71 instructions
(52% reduction in zero-operand instructions from 136 to 65).

Key changes:
- Track V_option and T_option size specifiers in template parser
- Detect single-letter register numbers (d, n, m, t, a)
- Add REG_SIMD_SIZED operand type with runtime size mapping
- Strip suffix numbers from link patterns for better matching

Test cases added for FCVTAS instruction. All existing tests pass.

https://bugs.webkit.org/show_bug.cgi?id=XXXXX
```

## Lessons Learned

1. **XML patterns are contextual**: Operands depend on preceding specifiers
2. **State tracking is essential**: Parser must remember context between operands
3. **Field mapping complexity**: Different instructions use different field names (sz, size, Q) for similar purposes
4. **Incremental approach works**: Fixing high-impact patterns first provides immediate value

## References

- ARM Architecture Reference Manual (ARM ARM)
- ARMv8-A XML instruction descriptions
- [Previous fixes]: MOVK_FIX.md, TST_TBNZ_FIX.md, LDRB_REG_OFFSET_FIX.md
