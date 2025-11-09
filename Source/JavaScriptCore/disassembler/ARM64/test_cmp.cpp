#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_cmp(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        bool match = (strstr(buffer, expected) != nullptr);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: %s\n", buffer);
            printf("  Expected: %s\n", expected);
        }
    } else {
        printf("✗ %s: Instruction not found! (0x%08X)\n", description, opcode);
    }
}

int main() {
    printf("Testing CMP instruction shift handling\n");
    printf("========================================\n\n");

    // CMP with immediate, no shift (should not show lsl)
    test_cmp("CMP x0, #0 (no shift)", 0xF100001F, "cmp      x0, #0");
    test_cmp("CMP x0, #1 (no shift)", 0xF100041F, "cmp      x0, #1");
    test_cmp("CMP w0, #0 (no shift)", 0x7100001F, "cmp      w0, #0");

    // CMP with immediate and shift (should show lsl #12)
    test_cmp("CMP x0, #0, lsl #12", 0xF140001F, "lsl #12");
    test_cmp("CMP x0, #1, lsl #12", 0xF140041F, "lsl #12");

    // CMP with register, no extend (should not show extend)
    test_cmp("CMP x0, x1 (no extend)", 0xEB01001F, "cmp      x0, x1");
    test_cmp("CMP w0, w1 (no extend)", 0x6B01001F, "cmp      w0, w1");

    // CMP with register and extend
    test_cmp("CMP x0, w1, uxtb", 0xEB21001F, "uxtb");
    test_cmp("CMP x0, w1, sxtw", 0xEB21C01F, "sxtw");

    return 0;
}
