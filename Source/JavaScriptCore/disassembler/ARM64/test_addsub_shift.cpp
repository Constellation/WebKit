#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_shift(const char* description, uint32_t opcode, bool should_have_shift) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        bool has_shift = (strstr(buffer, "lsl") != nullptr);
        bool match = (has_shift == should_have_shift);

        printf("%s: %s\n", match ? "✓" : "✗", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: %s\n", buffer);
            printf("  Expected %s shift\n", should_have_shift ? "WITH" : "WITHOUT");
        }
    } else {
        printf("✗ %s: Instruction not found! (0x%08X)\n", description, opcode);
    }
}

int main() {
    printf("Testing ADD/SUB immediate shift handling\n");
    printf("==========================================\n\n");

    printf("ADD instructions:\n");
    test_shift("ADD x0, x1, #0", 0x91000020, false);
    test_shift("ADD x0, x1, #1", 0x91000420, false);
    test_shift("ADD x0, x1, #0, lsl #12", 0x91400020, true);
    test_shift("ADD x0, x1, #1, lsl #12", 0x91400420, true);

    printf("\nSUB instructions:\n");
    test_shift("SUB x0, x1, #0", 0xD1000020, false);
    test_shift("SUB x0, x1, #1", 0xD1000420, false);
    test_shift("SUB x0, x1, #0, lsl #12", 0xD1400020, true);
    test_shift("SUB x0, x1, #1, lsl #12", 0xD1400420, true);

    printf("\nADDS instructions:\n");
    test_shift("ADDS x0, x1, #0", 0xB1000020, false);
    test_shift("ADDS x0, x1, #1", 0xB1000420, false);
    test_shift("ADDS x0, x1, #0, lsl #12", 0xB1400020, true);
    test_shift("ADDS x0, x1, #1, lsl #12", 0xB1400420, true);

    printf("\nSUBS instructions:\n");
    test_shift("SUBS x0, x1, #0", 0xF1000020, false);
    test_shift("SUBS x0, x1, #1", 0xF1000420, false);
    test_shift("SUBS x0, x1, #0, lsl #12", 0xF1400020, true);
    test_shift("SUBS x0, x1, #1, lsl #12", 0xF1400420, true);

    return 0;
}
