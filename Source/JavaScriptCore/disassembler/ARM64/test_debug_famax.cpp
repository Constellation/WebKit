#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Test existing working FAMAX opcodes
    uint32_t famax_half_q0 = 0x0ec01c00;  // Known working
    uint32_t famax_half_q1 = 0x4ec01c00;  // Should work (Q=1)
    
    printf("Testing existing FAMAX half:\n");
    printf("Q=0 (0x%08x): ", famax_half_q0);
    const auto* entry = findInstruction(famax_half_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_half_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    printf("Q=1 (0x%08x): ", famax_half_q1);
    entry = findInstruction(famax_half_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_half_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    // Test single precision
    uint32_t famax_single_q0 = 0x0ea1dc00;
    uint32_t famax_single_q1 = 0x4ea1dc00;
    
    printf("\nTesting FAMAX single:\n");
    printf("Q=0 (0x%08x): ", famax_single_q0);
    entry = findInstruction(famax_single_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_single_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    printf("Q=1 (0x%08x): ", famax_single_q1);
    entry = findInstruction(famax_single_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_single_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    return 0;
}
