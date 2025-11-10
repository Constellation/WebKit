#include <stdio.h>
#include <stdint.h>

static inline uint32_t extractBits(uint32_t value, unsigned start, unsigned width) {
    return (value >> start) & ((1U << width) - 1);
}

int main() {
    uint32_t opcode = 0x4ee1dc00;
    
    printf("Opcode: 0x%08x\n", opcode);
    printf("bits[28:24] = 0x%x (%u)\n", extractBits(opcode, 24, 5), extractBits(opcode, 24, 5));
    printf("bit[23]     = %u\n", extractBits(opcode, 23, 1));
    printf("bits[23:22] = %u (size)\n", extractBits(opcode, 22, 2));
    printf("bits[15:10] = 0x%x (%u)\n", extractBits(opcode, 10, 6), extractBits(opcode, 10, 6));
    printf("bit[30] (Q) = %u\n", extractBits(opcode, 30, 1));
    
    // Expected:
    // bits[28:24] = 0x0E (14)
    // bit[23] = 1
    // bits[23:22] = 3 (size=11)
    // bits[15:10] = 0x1B (27)
    // bit[30] = 1
    
    return 0;
}
