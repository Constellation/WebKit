#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // FAMIN half-precision (U=1, bits[23:22]=10, opcode=0111)
    uint32_t famin_half_q0 = 0x2ec01c00;
    uint32_t famin_half_q1 = 0x6ec01c00;
    
    printf("Testing FAMIN half-precision:\n");
    
    const auto* entry = findInstruction(famin_half_q0);
    printf("Q=0 (0x%08x): ", famin_half_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_half_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    entry = findInstruction(famin_half_q1);
    printf("Q=1 (0x%08x): ", famin_half_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_half_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    return 0;
}
