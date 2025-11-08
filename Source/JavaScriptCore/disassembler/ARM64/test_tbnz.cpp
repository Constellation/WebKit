#include "A64InstructionTable.h"
#include <stdio.h>
#include <stdint.h>

using namespace JSC::ARM64Disassembler;

int main() {
    printf("Testing TBNZ instruction:\n\n");

    // TBNZ w2, #5, <label>
    // Format: b5:b40 (bit position), imm14 (branch offset)
    // Test bit 5 and branch if not zero
    // Offset = +8 (2 instructions forward)
    // Bit 24 = 1 for TBNZ (0 for TBZ)

    uint32_t opcode1 = 0;
    opcode1 |= (0 << 31);       // b5=0 (bit 5, lower half)
    opcode1 |= (0b011011 << 25); // Fixed bits for test-and-branch
    opcode1 |= (1 << 24);       // op=1 for TBNZ (0 for TBZ)
    opcode1 |= (5 << 19);       // b40=5 (bits 4-0 of bit position)
    opcode1 |= (2 << 5);        // imm14=2 (offset +8 bytes = 2 instructions)
    opcode1 |= (2 << 0);        // Rt=2 (w2)

    printf("Test 1: TBNZ w2, #5, <+8>\n");
    printf("Opcode: 0x%08x\n", opcode1);
    printf("b5: %u\n", (opcode1 >> 31) & 1);
    printf("b40: %u\n", (opcode1 >> 19) & 0x1f);
    printf("Bit position: %u\n", ((opcode1 >> 31) & 1) * 32 + ((opcode1 >> 19) & 0x1f));
    printf("imm14: %u\n", (opcode1 >> 5) & 0x3fff);
    printf("Rt: %u\n", (opcode1 >> 0) & 0x1f);

    char buffer1[256];
    uint32_t* pc1 = &opcode1;
    const InstructionEntry* entry1 = findInstruction(opcode1);

    if (entry1) {
        printf("\nFound instruction: %s\n", entry1->name);
        printf("Operand count: %u\n", entry1->operandCount);

        // Show operand details
        for (unsigned i = 0; i < entry1->operandCount; i++) {
            const auto& op = g_operandTable[entry1->operandOffset + i];
            printf("Operand %u: type=%u, start=%u, width=%u, start2=%u, width2=%u\n",
                   i, op.type, op.field1_start, op.field1_width, op.field2_start, op.field2_width);
        }
    } else {
        printf("\nInstruction not found!\n");
    }

    formatInstruction(entry1, opcode1, pc1, nullptr, nullptr, buffer1, sizeof(buffer1));
    printf("\nFormatted: %s\n", buffer1);
    printf("Expected:  tbnz     w2, #5, <offset>\n");

    return 0;
}
