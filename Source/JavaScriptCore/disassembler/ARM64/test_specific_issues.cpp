#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    char buffer[256];
    uint32_t pc = 0;
    
    printf("Testing specific issues:\n\n");
    
    // 1. SDOT - what arrangement does it show?
    printf("1. SDOT arrangements:\n");
    uint32_t sdot_q0 = 0x0e019400; // size=00, Q=0
    uint32_t sdot_q1 = 0x4e019400; // size=00, Q=1
    
    auto* entry = findInstruction(sdot_q0);
    if (entry) {
        formatInstruction(entry, sdot_q0, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   SDOT Q=0: %s\n", buffer);
        printf("   Note: Shows input arrangement (.8b), not accumulator (.2s)\n");
    }
    
    entry = findInstruction(sdot_q1);
    if (entry) {
        formatInstruction(entry, sdot_q1, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   SDOT Q=1: %s\n", buffer);
        printf("   Note: Shows input arrangement (.16b), not accumulator (.4s)\n");
    }

    // 2. ORR - try different register combinations
    printf("\n2. ORR alias conditions:\n");

    // Try Rd=0, Rn=1, Rm=2
    uint32_t orr_diff = 0x0ea21c20; // FIXED: size=10 (ORR encoding)
    entry = findInstruction(orr_diff);
    if (entry) {
        formatInstruction(entry, orr_diff, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   ORR Rd=0,Rn=1,Rm=2: %s\n", buffer);
    }

    // Try Rd=0, Rn=0, Rm=1 (Rd==Rn, Rm different)
    uint32_t orr_rn_rd = 0x0ea11c00; // FIXED: size=10 (ORR encoding)
    entry = findInstruction(orr_rn_rd);
    if (entry) {
        formatInstruction(entry, orr_rn_rd, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   ORR Rd=0,Rn=0,Rm=1: %s\n", buffer);
    }

    // 3. MLA vs FMLAL - now fixed with correct opcodes
    printf("\n3. MLA element instruction (now using correct opcodes):\n");
    uint32_t mla_h = 0x2f420000; // FIXED: U=1, size=01, Rm=2, Q=0
    uint32_t mla_s = 0x2f820000; // FIXED: U=1, size=10, Rm=2, Q=0

    entry = findInstruction(mla_h);
    if (entry) {
        formatInstruction(entry, mla_h, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   0x%08x (size=01): %s\n", mla_h, buffer);
    }

    entry = findInstruction(mla_s);
    if (entry) {
        formatInstruction(entry, mla_s, &pc, nullptr, nullptr, buffer, sizeof(buffer));
        printf("   0x%08x (size=10): %s\n", mla_s, buffer);
    }
    
    return 0;
}
