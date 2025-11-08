#!/usr/bin/env python3
"""
Test harness for ARM64 disassembler

Tests various instruction encodings to validate formatter output.
"""

import subprocess
import os
import sys

# Test cases: (opcode, expected_output_substring)
TEST_CASES = [
    # ADD instructions
    (0x91000420, "add", "x0", "x1", "#1"),           # add x0, x1, #1
    (0x11000420, "add", "w0", "w1", "#1"),           # add w0, w1, #1
    (0x8b010020, "add", "x0", "x1", "x1"),           # add x0, x1, x1

    # SUB instructions
    (0xd1000420, "sub", "x0", "x1", "#1"),           # sub x0, x1, #1
    (0x51000420, "sub", "w0", "w1", "#1"),           # sub w0, w1, #1

    # MOV instructions
    (0xaa0103e0, "mov", "x0", "x1"),                 # mov x0, x1 (ORR)
    (0x2a0103e0, "mov", "w0", "w1"),                 # mov w0, w1 (ORR)
    (0xd2800000, "mov", "x0", "#0"),                 # mov x0, #0 (MOVZ)

    # Load/Store
    (0xf9400020, "ldr", "x0", "[x1]"),               # ldr x0, [x1]
    (0xb9400020, "ldr", "w0", "[x1]"),               # ldr w0, [x1]
    (0xf9000020, "str", "x0", "[x1]"),               # str x0, [x1]
    (0xb9000020, "str", "w0", "[x1]"),               # str w0, [x1]

    # Branches
    (0x14000001, "b"),                                # b +4
    (0x94000001, "bl"),                               # bl +4
    (0xd61f0000, "br", "x0"),                        # br x0
    (0xd63f0000, "blr", "x0"),                       # blr x0
    (0xd65f0000, "ret"),                             # ret (x30)

    # Conditional branch
    (0x54000001, "b.ne"),                            # b.ne +4
    (0x54000000, "b.eq"),                            # b.eq +4

    # Logical operations
    (0x8a010000, "and", "x0", "x0", "x1"),          # and x0, x0, x1
    (0xaa010000, "orr", "x0", "x0", "x1"),          # orr x0, x0, x1
    (0xca010000, "eor", "x0", "x0", "x1"),          # eor x0, x0, x1

    # FP operations
    (0x1e602000, "fmul", "d0", "d0", "d0"),         # fmul d0, d0, d0
    (0x1e202800, "fadd", "s0", "s0", "s0"),         # fadd s0, s0, s0
]

# Create minimal test C++ program
TEST_CPP = """
#include "A64InstructionTableV3.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t test_cases[] = {
"""

def generate_test_cpp():
    """Generate test C++ file"""
    cpp = TEST_CPP

    for opcode, *expected in TEST_CASES:
        cpp += f"        0x{opcode:08x},\n"

    cpp += """    };

    char buffer[256];

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t opcode = test_cases[i];
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\\n", opcode, buffer);
    }

    return 0;
}
"""
    return cpp

def main():
    if len(sys.argv) != 2:
        print("Usage: test_disassembler.py <output_directory>")
        sys.exit(1)

    output_dir = sys.argv[1]

    # Generate test program
    print("Generating test program...")
    test_cpp = generate_test_cpp()

    test_file = os.path.join(output_dir, 'test_disassembler.cpp')
    with open(test_file, 'w') as f:
        f.write(test_cpp)

    print(f"Generated test program: {test_file}")
    print(f"\nTo compile and run:")
    print(f"  cd {output_dir}")
    print(f"  clang++ -std=c++17 -DENABLE_ARM64_DISASSEMBLER=1 \\")
    print(f"          -I/path/to/webkit/Source/JavaScriptCore \\")
    print(f"          -I/path/to/webkit/Source/WTF \\")
    print(f"          test_disassembler.cpp A64InstructionTableV3.cpp \\")
    print(f"          -o test_disassembler")
    print(f"  ./test_disassembler")

    print(f"\nTest cases: {len(TEST_CASES)}")
    print("Testing various instruction types:")
    print("  - Arithmetic (ADD, SUB)")
    print("  - Data processing (MOV)")
    print("  - Load/Store (LDR, STR)")
    print("  - Branches (B, BL, BR, BLR, RET)")
    print("  - Conditional (B.cond)")
    print("  - Logical (AND, ORR, EOR)")
    print("  - Floating point (FMUL, FADD)")

if __name__ == '__main__':
    main()
