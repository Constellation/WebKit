#include "A64InstructionTable.h"
#include <stdio.h>
#include <string.h>

using namespace JSC::ARM64Disassembler;

int main() {
    uint32_t famax_half_q0 = 0x0ec01c00;
    uint32_t famax_single_q0 = 0x0ea1dc00;
    uint32_t famax_single_q1 = 0x4ea1dc00;

    printf("=== Testing FAMAX Opcodes ===\n\n");

    const auto* entry = findInstruction(famax_half_q0);
    printf("Half Q=0 (0x%08x): ", famax_half_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_half_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }

    entry = findInstruction(famax_single_q0);
    printf("Single Q=0 (0x%08x): ", famax_single_q0);
    if (entry) {
        char buf[256];
        uint32_t pc = 0;
        formatInstruction(entry, famax_single_q0, &pc, nullptr, nullptr, buf, sizeof(buf));
        printf("%s\n", buf);
    } else {
        printf("NOT FOUND\n");
    }

    entry = findInstruction(famax_single_q1);
    printf("Single Q=1 (0x%08x): ", famax_single_q1);
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
