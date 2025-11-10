#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* description;
        const char* expected;
    };

    TestCase tests[] = {
        // Bug 1: Incorrect imms field extraction (was using N instead of imms)
        { 0xb24003fa, "ORR/MOV #0x1 (Bug 1: was showing #0x3)", "   mov      x26, #0x1" },
        { 0xb2400020, "ORR x0, x1, #0x1", "   orr      x0, x1, #0x1" },

        // Bug 2: Undefined behavior with 64-bit rotation (1ULL << 64)
        { 0xb27c03e0, "ORR/MOV #0x10 (Bug 2: was showing #0x0)", "   mov      x0, #0x10" },
        { 0xb27c0000, "ORR x0, x0, #0x10", "   orr      x0, x0, #0x10" },

        // Additional logical immediate tests
        { 0xb2400400, "ORR/MOV #0x3", "   mov      x0, #0x3" },
        { 0xb2400800, "ORR/MOV #0x7", "   mov      x0, #0x7" },
        { 0xb2401000, "ORR/MOV #0xf", "   mov      x0, #0xf" },
        { 0xb2402000, "ORR/MOV #0x1f", "   mov      x0, #0x1f" },

        // 32-bit variants
        { 0x32000000, "ORR/MOV w0, #0x1", "   mov      w0, #0x1" },
        { 0x321c0000, "ORR/MOV w0, #0x10", "   mov      w0, #0x10" },
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    printf("Testing Logical Immediate Bug Fixes\n");
    printf("====================================\n\n");

    for (int i = 0; i < total; i++) {
        auto* entry = findInstruction(tests[i].opcode);
        if (entry) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry, tests[i].opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));

            bool match = strcmp(buffer, tests[i].expected) == 0;
            printf("%s: %s\n", match ? "✓" : "✗", tests[i].description);
            printf("  Opcode:   0x%08x\n", tests[i].opcode);
            printf("  Got:      %s\n", buffer);
            if (!match) {
                printf("  Expected: %s\n", tests[i].expected);
                printf("  *** MISMATCH ***\n");
            }
            printf("\n");

            if (match) passed++;
        } else {
            printf("✗: %s - No match found!\n\n", tests[i].description);
        }
    }

    printf("Result: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
