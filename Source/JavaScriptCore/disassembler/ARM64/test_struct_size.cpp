#include "A64InstructionTable.h"
#include <stdio.h>

int main() {
    printf("InstructionEntry size: %zu bytes\n", sizeof(JSC::ARM64Disassembler::InstructionEntry));
    printf("Expected: 24 bytes (8+4+4+2+2+1+1 = 22 + 2 padding)\n");
    return 0;
}
