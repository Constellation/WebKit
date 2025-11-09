#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_logical(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        bool match = (strcmp(buffer, expected) == 0);
        printf("%s: %s\n", match ? "OK" : "FAIL", description);
        if (!match) {
            printf("  Opcode: 0x%08X\n", opcode);
            printf("  Output: '%s'\n", buffer);
            printf("  Expect: '%s'\n", expected);
        }
    } else {
        printf("FAIL: %s - not found\n", description);
    }
}

int main() {
    printf("Testing logical operations with shift\n");
    printf("======================================\n\n");

    // No shift (imm6 = 0)
    test_logical("AND x0, x1, x2 (no shift)", 0x8A020020, "   and      x0, x1, x2");
    test_logical("ORR x0, x1, x2 (no shift)", 0xAA020020, "   orr      x0, x1, x2");
    test_logical("EOR x0, x1, x2 (no shift)", 0xCA020020, "   eor      x0, x1, x2");

    // With shift (imm6 != 0)
    test_logical("AND x0, x1, x2, lsl #4", 0x8A021020, "   and      x0, x1, x2, lsl #4");
    test_logical("ORR x0, x1, x2, lsr #8", 0xAA422020, "   orr      x0, x1, x2, lsr #8");
    test_logical("EOR x0, x1, x2, asr #12", 0xCA823020, "   eor      x0, x1, x2, asr #12");

    return 0;
}
