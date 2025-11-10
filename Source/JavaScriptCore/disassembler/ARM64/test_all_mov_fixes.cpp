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
        // MOVN tests (opc=00)
        { 0x92800002, "MOVN x2, #0 (=-1)", "   mov      x2, #-1" },
        { 0x92800020, "MOVN x0, #1 (=-2)", "   mov      x0, #-2" },
        { 0x92800040, "MOVN x0, #2 (=-3)", "   mov      x0, #-3" },
        { 0x928001e0, "MOVN x0, #15 (=-16)", "   mov      x0, #-16" },

        // MOVZ tests (opc=10) - should still work
        { 0xd2800020, "MOVZ x0, #1", "   mov      x0, #0x1" },
        { 0xd280014a, "MOVZ x10, #10", "   mov      x10, #0xa" },

        // Logical immediate tests - should still work
        { 0xb24003fa, "ORR/MOV #0x1", "   mov      x26, #0x1" },
        { 0xb27c03e0, "ORR/MOV #0x10", "   mov      x0, #0x10" },
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    printf("Testing MOVN Fix and Previous Fixes\n");
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
