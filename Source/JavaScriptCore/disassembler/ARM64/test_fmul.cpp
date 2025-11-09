#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_fmul(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        // Check if output contains expected substring
        bool match = (strstr(buffer, expected) != nullptr);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        printf("  Opcode: 0x%08X\n", opcode);
        printf("  Output: %s\n", buffer);
        printf("  Expect: %s\n", expected);
        if (!match) {
            printf("  MISMATCH!\n");
        }
        printf("\n");
    } else {
        printf("✗ %s: Instruction not found!\n", description);
        printf("  Opcode: 0x%08X\n\n", opcode);
    }
}

int main() {
    printf("Testing FMUL instruction with all arrangement variants\n");
    printf("======================================================\n\n");

    // FMUL (vector) - Half-precision (assembled opcodes from objdump)
    // Q=0 → 4H, Q=1 → 8H
    test_fmul("FMUL 4H (Q=0)", 0x2E401C00, "fmul");
    test_fmul("FMUL 8H (Q=1)", 0x6E401C00, "fmul");

    // FMUL (vector) - Single/Double precision (assembled opcodes from objdump)
    // sz=0, Q=0 → 2S
    // sz=0, Q=1 → 4S
    // sz=1, Q=1 → 2D
    test_fmul("FMUL 2S (sz=0, Q=0)", 0x2E20DC00, "fmul      v0.2s, v0.2s, v0.2s");
    test_fmul("FMUL 4S (sz=0, Q=1)", 0x6E20DC00, "fmul      v0.4s, v0.4s, v0.4s");
    test_fmul("FMUL 2D (sz=1, Q=1)", 0x6E60DC00, "fmul      v0.2d, v0.2d, v0.2d");

    // Test with different registers
    test_fmul("FMUL v1.4s, v2.4s, v3.4s", 0x6E23DC41, "fmul      v1.4s, v2.4s, v3.4s");

    printf("Testing complete!\n");
    return 0;
}
