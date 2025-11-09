#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_ldrb_shift(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        bool match = (strcmp(buffer, expected) == 0);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: '%s'\n", buffer);
            printf("  Expect: '%s'\n", expected);
        }
    } else {
        printf("✗: %s - instruction not found\n", description);
    }
}

int main() {
    printf("Testing LDRB/LDRH/LDR shift display fix\n");
    printf("========================================\n\n");

    // LDRB with no shift (S=0)
    test_ldrb_shift("LDRB no shift (S=0)", 0x38606ac0, "   ldrb     w0, [x22, x0]");

    // LDRB with S=1 but shift amount = 0 (byte access)
    // This is the key test case - should NOT show "lsl #0"
    test_ldrb_shift("LDRB S=1 zero shift", 0x38607ac0, "   ldrb     w0, [x22, x0]");

    // LDRH with S=1, shift amount = 1 (halfword access)
    test_ldrb_shift("LDRH lsl #1", 0x78607ac0, "   ldrh     w0, [x22, x0, lsl #1]");

    // LDR W with S=1, shift amount = 2 (word access)
    test_ldrb_shift("LDR W lsl #2", 0xb8607ac0, "   ldr      w0, [x22, x0, lsl #2]");

    // LDR X with S=1, shift amount = 3 (doubleword access)
    test_ldrb_shift("LDR X lsl #3", 0xf8607ac0, "   ldr      x0, [x22, x0, lsl #3]");

    printf("\nAll LDRB shift tests completed\n");
    return 0;
}
