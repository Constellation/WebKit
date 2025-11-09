#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

void test_tbl(const char* description, uint32_t opcode, const char* expected) {
    char buffer[256];
    uint32_t pc = 0;
    memset(buffer, 0, sizeof(buffer));

    auto* entry = ARM64Disassembler::findInstruction(opcode);
    if (entry) {
        ARM64Disassembler::formatInstruction(entry, opcode, &pc, &pc, &pc, buffer, sizeof(buffer));

        // Check if output matches expected
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
    printf("Testing TBL instruction variants\n");
    printf("=================================\n\n");

    // TBL single register: tbl v1.8b, { v0.16b }, v0.8b
    // Q=0, len=00, Rd=1, Rn=0, Rm=0
    test_tbl("TBL 1 register", 0x0E000001, "tbl      v1.8b, { v0.16b }, v0.8b");

    // TBL two registers: tbl v1.8b, { v0.16b, v1.16b }, v0.8b
    // Q=0, len=01, Rd=1, Rn=0, Rm=0
    test_tbl("TBL 2 registers", 0x0E002001, "tbl      v1.8b, { v0.16b, v1.16b }, v0.8b");

    // TBL three registers: tbl v1.8b, { v0.16b, v1.16b, v2.16b }, v0.8b
    // Q=0, len=10, Rd=1, Rn=0, Rm=0
    test_tbl("TBL 3 registers", 0x0E004001, "tbl      v1.8b, { v0.16b, v1.16b, v2.16b }, v0.8b");

    // TBL four registers: tbl v1.8b, { v0.16b, v1.16b, v2.16b, v3.16b }, v0.8b
    // Q=0, len=11, Rd=1, Rn=0, Rm=0
    test_tbl("TBL 4 registers", 0x0E006001, "tbl      v1.8b, { v0.16b, v1.16b, v2.16b, v3.16b }, v0.8b");

    // TBL with Q=1 (16b arrangement): tbl v1.16b, { v0.16b }, v0.16b
    // Q=1, len=00, Rd=1, Rn=0, Rm=0
    test_tbl("TBL Q=1", 0x4E000001, "tbl      v1.16b, { v0.16b }, v0.16b");

    return 0;
}
