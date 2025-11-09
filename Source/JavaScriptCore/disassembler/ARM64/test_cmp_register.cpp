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

        bool match = (strcmp(buffer, expected) == 0);
        printf("%s: %s\n", match ? "✓" : "✗", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: '%s'\n", buffer);
            printf("  Expect: '%s'\n", expected);
        }
    } else {
        printf("X: %s: Instruction not found!\n", description);
    }
}

int main() {
    printf("Testing CMP register forms\n");
    printf("============================\n\n");

    // CMP shifted register - NO shift (imm6 = 0)
    test_cmp("CMP x0, x1 (no shift)", 0xEB01001F, "   cmp      x0, x1");
    test_cmp("CMP w0, w1 (no shift)", 0x6B01001F, "   cmp      w0, w1");

    // CMP shifted register - WITH shift (imm6 != 0)
    test_cmp("CMP x0, x1, lsl #4", 0xEB01101F, "   cmp      x0, x1, lsl #4");
    test_cmp("CMP x0, x1, lsr #4", 0xEB41101F, "   cmp      x0, x1, lsr #4");
    test_cmp("CMP x0, x1, asr #4", 0xEB81101F, "   cmp      x0, x1, asr #4");
    test_cmp("CMP w0, w1, lsl #4", 0x6B01101F, "   cmp      w0, w1, lsl #4");

    // CMP extended register - NO shift amount (imm3 = 0)
    test_cmp("CMP x0, w1, uxtb", 0xEB21001F, "   cmp      x0, w1, uxtb");
    test_cmp("CMP x0, w1, sxtw", 0xEB21C01F, "   cmp      x0, w1, sxtw");

    // CMP extended register - WITH shift amount (imm3 != 0)
    test_cmp("CMP x0, w1, sxtw #2", 0xEB21C81F, "   cmp      x0, w1, sxtw #2");

    return 0;
}
