#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    struct TestCase {
        uint32_t opcode;
        const char* expected;
    };

    TestCase tests[] = {
        // MOV with no shift (hw=0)
        { 0xd2800060, "   mov      x0, #0x3" },
        { 0xd280003a, "   mov      x26, #0x1" },
        { 0xd280014a, "   mov      x10, #0xa" },
        { 0xd28001e0, "   mov      x0, #0xf" },

        // MOV with shift (hw!=0) - shows as mov with explicit shift
        { 0xd2a0014a, "   mov      x10, #0xa, lsl #16" },
        { 0xd2c0016b, "   mov      x11, #0xb, lsl #32" },
        { 0xd2e0018c, "   mov      x12, #0xc, lsl #48" },

        // 32-bit variants
        { 0x52800060, "   mov      w0, #0x3" },
        { 0x52800140, "   mov      w0, #0xa" },
    };

    int passed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    printf("Testing MOV/MOVZ immediate handling\n");
    printf("====================================\n\n");

    for (int i = 0; i < total; i++) {
        auto* entry = findInstruction(tests[i].opcode);
        if (entry) {
            char buffer[256];
            uint32_t pc = 0;
            formatInstruction(entry, tests[i].opcode, &pc, nullptr, nullptr, buffer, sizeof(buffer));

            bool match = strcmp(buffer, tests[i].expected) == 0;
            printf("%s: 0x%08x\n", match ? "✓" : "✗", tests[i].opcode);
            printf("  Got:      %s\n", buffer);
            printf("  Expected: %s\n", tests[i].expected);
            if (!match) {
                printf("  *** MISMATCH ***\n");
            }
            printf("\n");

            if (match) passed++;
        } else {
            printf("✗: 0x%08x - No match found!\n\n", tests[i].opcode);
        }
    }

    printf("Result: %d/%d tests passed\n", passed, total);
    return (passed == total) ? 0 : 1;
}
