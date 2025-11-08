#!/usr/bin/env python3
"""
Test fp (x29) and lr (x30) register alias formatting
"""

import subprocess
import os
import sys

# Test cases with x29 (fp) and x30 (lr)
TEST_CASES = [
    # Instructions using x29 (fp) and x30 (lr)
    (0xa9bf7bfd, "stp", "fp", "lr"),          # stp x29, x30, [sp, #-16]!
    (0xa8c17bfd, "ldp", "fp", "lr"),          # ldp x29, x30, [sp], #16
    (0x910003fd, "mov", "fp", "sp"),          # mov x29, sp
    (0xd65f03c0, "ret", "lr"),                # ret x30 (lr)

    # More fp/lr instructions
    (0xf81f0ffe, "str", "lr"),                # str x30, [sp, #-16]!
    (0xf94003fd, "ldr", "fp"),                # ldr x29, [sp]
    (0x8b1d03e0, "add", "x0", "fp"),          # add x0, sp, x29

    # Regular registers for comparison
    (0x91000420, "add", "x0", "x1"),          # add x0, x1, #1
    (0x8b010000, "add", "x0", "x0", "x1"),    # add x0, x0, x1
]

TEST_CPP = """
#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t test_cases[] = {
"""

def generate_test_cpp():
    cpp = TEST_CPP

    for opcode, *expected in TEST_CASES:
        cpp += f"        0x{opcode:08x},\n"

    cpp += """    };

    const char* descriptions[] = {
"""

    for opcode, *expected in TEST_CASES:
        expected_str = ", ".join(expected)
        cpp += f'        "{expected_str}",\n'

    cpp += """    };

    char buffer[256];
    int passed = 0;
    int failed = 0;

    printf("Testing fp/lr register alias formatting:\\n\\n");

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t opcode = test_cases[i];
        uint32_t* pc = &opcode;

        const InstructionEntry* entry = findInstruction(opcode);
        formatInstruction(entry, opcode, pc, nullptr, nullptr, buffer, sizeof(buffer));

        printf("0x%08x: %s\\n", opcode, buffer);

        // Check if expected strings are in output
        bool all_found = true;
        const char* desc = descriptions[i];
        char desc_copy[256];
        strncpy(desc_copy, desc, sizeof(desc_copy));

        char* token = strtok(desc_copy, ",");
        while (token) {
            // Trim whitespace
            while (*token == ' ') token++;
            char* end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = 0;

            if (strstr(buffer, token) == nullptr) {
                all_found = false;
                printf("  ✗ Missing: '%s'\\n", token);
            }
            token = strtok(nullptr, ",");
        }

        if (all_found) {
            printf("  ✓ Contains: %s\\n\\n", descriptions[i]);
            passed++;
        } else {
            printf("  Expected: %s\\n\\n", descriptions[i]);
            failed++;
        }
    }

    printf("====================\\n");
    printf("Results: %d passed, %d failed\\n", passed, failed);

    return (failed > 0) ? 1 : 0;
}
"""
    return cpp

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output_directory>")
        sys.exit(1)

    output_dir = sys.argv[1]

    print("Generating fp/lr register alias test...")
    test_cpp = generate_test_cpp()

    test_file = os.path.join(output_dir, 'test_fp_lr.cpp')
    with open(test_file, 'w') as f:
        f.write(test_cpp)

    print(f"Generated test program: {test_file}")
    print(f"\nTest cases: {len(TEST_CASES)}")
    print("Testing:")
    print("  - x29 -> fp (frame pointer)")
    print("  - x30 -> lr (link register)")

if __name__ == '__main__':
    main()
