# ARM64 Disassembler Operand Coverage Analysis

## Executive Summary

The current parser handles **45 out of 605** unique operand link patterns found in ARM64 XML files (7.4% coverage). This results in **136 instructions with zero operands** (3.4% of 4,013 instructions).

## Current Status

### What's Working ✅
- **GP Registers**: Basic X and W register patterns (xd, xn, xm, wd, wn, wm)
- **FP Registers**: Basic floating-point registers (bd, hd, sd, dd, qd)
- **SVE Registers**: Basic SVE patterns (zd, zn, zm, pd, pn)
- **Immediates**: Basic immediate and shift patterns
- **Memory**: Basic memory addressing modes
- **Core Instructions**: ADD, MOV, MOVK, LDRB (with fixes), TST, TBNZ

### What's Missing ❌

#### 1. **Case Sensitivity Issues** (HIGH PRIORITY)
Parser checks lowercase patterns (e.g., `hd`) but XML uses mixed case (e.g., `Hd`, `HD`).

**Example**: FCVTAS instruction has:
- Link: `Hd` (capital H) - NOT MATCHED
- Link: `Hn__2` (capital H) - NOT MATCHED

**Impact**: Many FP/SIMD instructions fail to parse operands.

**Fix**: Make pattern matching case-insensitive.

#### 2. **Single-Letter Links** (HIGH PRIORITY)
XML uses single letters for register numbers with size specifiers.

**Examples**:
- `d` - destination register number
- `n`, `n__3` - source register number
- `t`, `t__2` - transfer register number

**Context**: Used with size specifiers like `V_option` which determines register width.

**Fix**: Detect single-letter patterns and pair with preceding size specifier.

#### 3. **Option/Specifier Patterns** (MEDIUM PRIORITY)
**Count**: 107 unique patterns

**Examples**:
- `V_option__9` - SIMD register size specifier (B/H/S/D)
- `T_option` - Element size specifier
- `s_2_option` - Size option variants

**Purpose**: Determine element sizes in SIMD/vector operations.

**Impact**: Without these, SIMD vector instructions show no operands or incorrect formatting.

**Fix**: Parse option patterns to determine register types/sizes.

#### 4. **SVE/SME Patterns** (LOW PRIORITY for JavaScriptCore)
**Count**: Majority of "OTHER" category (~200+ patterns)

**Prefix**: `sa_*` (Scalable Architecture)

**Examples**:
- `sa_zd`, `sa_zn` - SVE vector registers
- `sa_pg` - SVE predicate registers
- `sa_imm` - SVE immediates

**Recommendation**: Defer SVE/SME support - rarely used in JavaScript engines.

#### 5. **Vector Register Lists** (MEDIUM PRIORITY)
**Examples**:
- `Vt`, `Vt2`, `Vt3`, `Vt4` - Vector register lists
- Used in load/store multiple (LD2, LD3, LD4, ST2, ST3, ST4)

**Impact**: Multi-vector load/store instructions show incomplete operands.

#### 6. **Register Variants with Suffixes** (HIGH PRIORITY)
**Count**: Most register patterns have numbered variants

**Examples**:
- `XdOrXZR__2`, `XdOrXZR__6` - X register or XZR with variant numbers
- `WtOrWZR__4` - W register or WZR variants
- `Hn__2`, `Sd__3` - FP register variants

**Current**: We check `xdorzr` but not with suffixes.

**Fix**: Strip suffix numbers before pattern matching.

## Statistics

```
Total XML files:           2,136
Total instructions:        4,013
Instructions with 0 ops:     136 (3.4%)
Instructions with 1 op:      368 (9.2%)
Instructions with 2+ ops:  3,509 (87.4%)

Unique link patterns:        605
Currently handled:            45 (7.4%)
```

## Breakdown by Instruction Type

### Zero-Operand Instructions by Category:
```
Pointer Authentication: 21 (pac*, aut*)
FP Conversion:          11 (fcvtas*, fcvtns*, etc.)
FP Compare:             12 (fcmgt*, fcmge*, fcmle*, etc.)
SIMD Saturating:         8 (sqd*, sqr*, sqx*)
Hints/System:           20 (yield, wfe, sev, etc.)
Other:                  64
```

### High-Impact Missing Patterns:
```
Pattern Type              Count    Priority    Instructions Affected
--------------------------------------------------------------------------------
Case variants             ~300     HIGH        FP/SIMD scalar operations
Single-letter links       ~50      HIGH        SIMD operations with size specs
Option/specifiers         107      MEDIUM      Vector element operations
SVE (sa_* prefix)         200+     LOW         Advanced SIMD (rare in JS)
Register suffixes         ~100     HIGH        All register variants
```

## Recommended Fix Strategy

### Phase 1: Foundation Fixes (HIGH PRIORITY)
**Impact**: ~80% of missing operands

1. **Case-Insensitive Matching**
   ```python
   # Before: link_lower.startswith('hd')
   # After:  link_lower.startswith(('hd', 'Hd', 'HD'))
   # Better: Normalize to lowercase first
   ```

2. **Strip Suffix Numbers**
   ```python
   # Before: link_lower == 'xdorzr'
   # After:  link_base = re.sub(r'__\d+$', '', link_lower)
   ```

3. **Single-Letter Register Numbers**
   ```python
   # Detect patterns like: V_option, <d>
   # Where 'd' is register number and V_option determines size
   ```

### Phase 2: SIMD/Vector Support (MEDIUM PRIORITY)
**Impact**: ~15% of missing operands

1. **Option/Specifier Parsing**
   - Parse V_option, T_option to determine element sizes
   - Map to appropriate register types

2. **Vector Register Lists**
   - Handle Vt, Vt2, Vt3, Vt4 patterns
   - Format as register ranges

### Phase 3: Advanced Features (LOW PRIORITY)
**Impact**: ~5% of missing operands

1. **SVE/SME Support** - Defer unless needed
2. **Specialized SIMD** - Matrix operations, etc.

## Implementation Approach

### Option A: Systematic Pattern Addition (Recommended for User)
**Effort**: High
**Coverage**: 90%+
**Maintainability**: Good

1. Create pattern normalization function
2. Build comprehensive pattern database
3. Add patterns category by category
4. Test each category

### Option B: Targeted Fixes (Current Approach)
**Effort**: Medium per fix
**Coverage**: Incremental
**Maintainability**: Medium

Continue fixing patterns as issues are discovered (current approach).

### Option C: Template-Based Parsing
**Effort**: Very High (redesign)
**Coverage**: 95%+
**Maintainability**: Excellent

Parse entire asmtemplate as structured data, not pattern matching.

## Next Steps

Given the scope (605 patterns, 136 affected instructions), I recommend:

1. **Immediate**: Fix case sensitivity and suffix handling (Phase 1)
   - Impact: ~100 of 136 zero-operand instructions fixed
   - Effort: 1-2 hours

2. **Short-term**: Add SIMD option parsing (Phase 2)
   - Impact: ~30 more instructions fixed
   - Effort: 2-3 hours

3. **Long-term**: Document remaining patterns for future enhancement
   - SVE/SME can be added when needed
   - Framework in place for easy addition

## Files for Reference

- `analyze_operands.py` - Extracts all link patterns from XML
- `check_parser_coverage.py` - Shows currently handled patterns
- `analyze_table.py` - Identifies instructions with missing operands

## Examples of Missing Operands

### FCVTAS (FP Convert to Signed - scalar)
```
XML: <Hd>, <Hn>
Links: Hd, Hn__2
Issue: Capital 'H' not matched
Current: fcvtas   (no operands)
Expected: fcvtas   h0, h1
```

### FCVTAS (FP Convert - vector with size)
```
XML: <V><d>, <V><n>
Links: V_option__9, d, V_option__9, n__3
Issue: Single-letter 'd' and 'n' not handled
Current: fcvtas   (no operands)
Expected: fcvtas   v0.2s, v1.2s  (with size from V_option)
```

### SQADD (SIMD Saturating Add)
```
XML: <V><d>, <V><n>, <V><m>
Links: V_option, d, n, m
Issue: Single-letter links + size specifier
Current: sqadd    (no operands)
Expected: sqadd    v0.8b, v1.8b, v2.8b
```

---

**Date**: November 8, 2025
**Analysis**: Comprehensive operand pattern coverage review
**Total Patterns**: 605 (45 handled, 560 missing)
**Recommendation**: Phased approach starting with case/suffix fixes
