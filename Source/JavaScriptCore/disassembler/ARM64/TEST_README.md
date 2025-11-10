# ARM64 Disassembler Test Suite

## Quick Start

Run all tests:
```bash
./run_tests.sh
```

Or manually:
```bash
clang++ -std=c++20 -I. -o test_disassembler test_disassembler.cpp A64InstructionTable.cpp
./test_disassembler
```

## Test Coverage

The unified test suite (`test_disassembler.cpp`) is a comprehensive collection of test cases extracted from all individual test files. It covers:

### Core Bug Fixes

#### Bug 1: Logical Immediate Field Extraction
- **Issue**: `0xb24003fa` showed `#0x3` instead of `#0x1`
- **Fix**: Correctly parse "N:imms:immr" encoding
- **Tests**: 5 test cases covering ORR/MOV with logical immediates

#### Bug 2: 64-bit Rotation in Logical Immediate
- **Issue**: `0xb27c03e0` showed `#0x0` instead of `#0x10`
- **Fix**: Avoid undefined behavior with `(1ULL << 64)`
- **Tests**: Included in logical immediate tests

#### Bug 3: MOVN Immediate Display
- **Issue**: `0x92800002` showed `#0x0` instead of `#-1`
- **Fix**: Compute and display `~(imm16 << (hw*16))` for MOVN
- **Tests**: 5 test cases for MOVN with various values

#### Bug 4: ADD/SUB/CMP Hex Formatting
- **Issue**: `0x7100281f` showed decimal `#10` instead of hex `#0xa`
- **Fix**: Display 12-bit ADD/SUB/CMP immediates in hex
- **Tests**: Multiple test cases for ADD/SUB/CMP with immediates

### Test Categories (189 Total Tests)

#### General Purpose Instructions
- **Logical Imm** (5 tests): ORR/MOV with logical immediates
- **MOVN** (5 tests): Move with NOT (negative values)
- **MOVZ** (7 tests): Move with zero
- **MOVZ Shift** (3 tests): Move with shift amounts
- **CMP** (9 tests): Compare immediate with hex formatting
- **CMP Reg** (9 tests): Compare register with shifts/extends
- **ADD** (6 tests): Add immediate
- **ADD Shift** (4 tests): Add with lsl #12
- **ADD Extend** (2 tests): Add with register extend
- **ADD Flags** (4 tests): ADDS instruction
- **SUB** (5 tests): Subtract immediate
- **SUB Shift** (3 tests): Subtract with lsl #12
- **SUB Flags** (4 tests): SUBS instruction
- **Shift** (4 tests): LSL, LSR, ASR operations
- **TST** (1 test): Test bits
- **TBNZ** (1 test): Test and branch
- **Atomic** (1 test): CASAL

#### Memory Operations
- **Memory** (14 tests): LDR, LDUR, STP, LDP with various addressing modes
- **Memory Reg** (12 tests): Register offset addressing with extends

#### Floating Point
- **FP Convert** (2 tests): FCVTAS
- **FP Move** (2 tests): FMOV immediate

#### SIMD Arithmetic
- **SIMD Add** (7 tests): All arrangements (.8b, .16b, .4h, .8h, .2s, .4s, .2d)
- **SIMD Sub** (2 tests): Vector subtract
- **SIMD Mul** (6 tests): Vector multiply
- **SIMD Logic** (6 tests): AND, ORR, EOR with Q-bit variants

#### SIMD Floating Point
- **SIMD FP** (14 tests): FAMAX, FAMIN, FMUL with all precisions

#### SIMD Data Movement
- **DUP** (8 tests): Duplicate element to vector (all element sizes)
- **UMOV** (5 tests): Unsigned move from vector element
- **INS** (5 tests): Insert element from GPR
- **MOV Elem** (5 tests): Element-to-element move

#### SIMD Extensions
- **SXTL** (6 tests): Sign extend long with Q-bit variants
- **UXTL** (6 tests): Unsigned extend long with Q-bit variants

#### SIMD Table Operations
- **TBL** (3 tests): Table lookup
- **TBX** (2 tests): Table extension
- **EXT** (2 tests): Extract

#### SIMD Load/Store
- **LD1** (8 tests): Load multiple structures with all arrangements

## Adding New Tests

To add a new test, edit `test_disassembler.cpp` and add an entry to the `tests` array:

```cpp
{ 0x12345678, "Description", "   expected   output", "Category" },
```

Then run:
```bash
./run_tests.sh
```

## Test Statistics

- **Total Tests**: 189
- **Categories**: 36 (General Purpose, Memory, FP, SIMD Arithmetic, SIMD FP, SIMD Data Movement, SIMD Extensions, SIMD Table, SIMD Load/Store)
- **Current Status**: ⚠️  154/189 tests passing (81.5%)

### Known Issues (35 failing tests)

The following test patterns are currently failing and may indicate disassembler bugs or alias priority issues:

1. **DUP Instructions**: Source register showing as v0 instead of v1
   - Example: `DUP v0.8b, v1.b[0]` → Output: `dup v0.8b, v0.b[0]`

2. **UMOV Aliasing**: S and D element moves aliasing to MOV instead of UMOV
   - Example: `UMOV w0, v0.s[0]` → Output: `mov w0, v0.s[0]`

3. **INS vs MOV Element**: Element-to-element moves showing confusion between INS and MOV
   - Some INS instructions showing as MOV element
   - Some MOV element instructions showing as INS

4. **TBL Operands**: Last operand register mismatch
   - Example: `TBL v1.8b, {v0.16b}, v1.8b` → Wrong last operand shown

These issues are under investigation and do not affect the core bug fixes (logical immediate, MOVN, hex formatting).

## Files

- `test_disassembler.cpp` - **Unified test suite** (189 comprehensive tests)
- `run_tests.sh` - Quick test runner script
- `A64InstructionTable.cpp` - Generated disassembler (do not edit manually)
- `generate_arm64_disassembler.py` - Generator script for A64InstructionTable.cpp
- `test_*.cpp`, `verify_*.cpp`, `analyze_*.cpp` - Original source test files (86 files, now superseded by unified suite)
