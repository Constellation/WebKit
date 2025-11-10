#include "A64InstructionTable.h"
#include <stdio.h>

using namespace JSC::ARM64Disassembler;

int main() {
    // Correct FAMIN single opcodes (U=1)
    uint32_t famin_single_q0 = 0x2ea1dc00;  // Changed from 0x0ea19c00
    uint32_t famin_single_q1 = 0x6ea1dc00;  // Changed from 0x4ea19c00
    
    printf("Testing FAMIN single with correct opcodes:\n");
    
    const auto* entry = findInstruction(famin_single_q0);
    printf("Q=0 (0x%08x): ", famin_single_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_single_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    entry = findInstruction(famin_single_q1);
    printf("Q=1 (0x%08x): ", famin_single_q1);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famin_single_q1, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }
    
    return 0;
}
