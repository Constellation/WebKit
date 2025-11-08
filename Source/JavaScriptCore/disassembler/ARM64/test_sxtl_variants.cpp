#include "A64InstructionTable.h"
#include <cstdio>
#include <cstring>

using namespace JSC;

int main() {
    char buffer[256];
    uint32_t pc = 0;

    printf("Testing SXTL variants:\n\n");

    // SXTL v1.8h, v0.8b (immh=0001, Q=0)
    uint32_t opcode1 = 0x0F08A401;
    memset(buffer, 0, sizeof(buffer));
    auto* entry1 = ARM64Disassembler::findInstruction(opcode1);
    if (entry1) {
        ARM64Disassembler::formatInstruction(entry1, opcode1, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode1, buffer);
        printf("Expected:    sxtl     v1.8h, v0.8b\n\n");
    }

    // SXTL v1.4s, v0.4h (immh=0010, Q=0)
    uint32_t opcode2 = 0x0F10A401;
    memset(buffer, 0, sizeof(buffer));
    auto* entry2 = ARM64Disassembler::findInstruction(opcode2);
    if (entry2) {
        ARM64Disassembler::formatInstruction(entry2, opcode2, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode2, buffer);
        printf("Expected:    sxtl     v1.4s, v0.4h\n\n");
    }

    // SXTL2 v1.8h, v0.16b (immh=0001, Q=1)
    uint32_t opcode3 = 0x4F08A401;
    memset(buffer, 0, sizeof(buffer));
    auto* entry3 = ARM64Disassembler::findInstruction(opcode3);
    if (entry3) {
        ARM64Disassembler::formatInstruction(entry3, opcode3, &pc, &pc, &pc, buffer, sizeof(buffer));
        printf("0x%08x: %s\n", opcode3, buffer);
        printf("Expected:    sxtl2    v1.8h, v0.16b\n\n");
    }

    return 0;
}
